#include "manager/service/PublishService.h"

#include "manager/announce/AnnounceCodec.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <system_error>
#include <utility>

namespace wgrd::manager {

namespace {

constexpr std::string_view KEY_FILE = "publisher.key";
constexpr std::string_view REGISTRY_FOLDER = "registry";
constexpr std::string_view MANIFEST_FOLDER = "manifests";

}

PublishService::PublishService(
    std::filesystem::path modsDirectory,
    std::filesystem::path dataDirectory,
    const domain::IContentChunker& chunker,
    const domain::IContentHasher& hasher,
    const domain::IPayloadPathPolicy& pathPolicy,
    const domain::IDataProtection& protection,
    const domain::IChunkSetTorrentBuilder& torrentBuilder,
    domain::IAnnounceReceiver& receiver)
    : _modsDirectory(std::move(modsDirectory)),
      _dataDirectory(std::move(dataDirectory)),
      _hasher(&hasher),
      _torrentBuilder(&torrentBuilder),
      _receiver(&receiver),
      _keyStore(_dataDirectory / KEY_FILE, protection),
      _exporter(_dataDirectory / REGISTRY_FOLDER),
      _builder(chunker, hasher, pathPolicy),
      _codec(pathPolicy),
      _signer(_keyStore),
      _announcer(_keyStore, hasher),
      _store(_dataDirectory / MANIFEST_FOLDER),
      _publisher{false, {}, {}},
      _candidates(),
      _history(),
      _versions(),
      _message() {

    RefreshPublisher_();
    RefreshCandidates();
}

PublishService::~PublishService() = default;

std::string PublishService::Today_() {
    const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    return std::format("{:%Y-%m-%d}", today);
}

void PublishService::RefreshPublisher_() {
    if (!_keyStore.Exists()) {
        _publisher = domain::PublisherState{false, {}, {}};
        return;
    }

    const auto identity = _keyStore.Identity();
    if (!identity.has_value()) {
        _publisher = domain::PublisherState{false, {}, {}};
        _message = "key unreadable";
        return;
    }

    _publisher = domain::PublisherState{true, identity->fingerprint.ToHex(), _publisher.name};
}

const domain::PublisherState& PublishService::Publisher() const {
    return _publisher;
}

const ManifestStore& PublishService::Store() const {
    return _store;
}

std::expected<domain::PublisherState, domain::PublishError> PublishService::CreateKey(
    std::string_view publisherName) {

    if (_keyStore.Exists()) {
        _message = "key already present";
        return std::unexpected(domain::PublishError::KeyAlreadyPresent);
    }

    const auto identity = _keyStore.Create();
    if (!identity.has_value()) {
        _message = "key create failed";
        return std::unexpected(domain::PublishError::KeyCreateFailed);
    }

    const auto exported = _exporter.Export(*identity, publisherName, Today_());
    if (!exported.has_value()) {
        _message = "key export failed";
        return std::unexpected(domain::PublishError::PublisherNameRejected);
    }

    _publisher = domain::PublisherState{true, identity->fingerprint.ToHex(), std::string(publisherName)};
    _message = "key created";

    return _publisher;
}

const std::vector<std::string>& PublishService::Candidates() const {
    return _candidates;
}

void PublishService::RefreshCandidates() {
    _candidates.clear();

    std::error_code failure;
    if (!std::filesystem::is_directory(_modsDirectory, failure)) {
        return;
    }

    std::filesystem::directory_iterator walker(_modsDirectory, failure);
    if (failure) {
        return;
    }

    const std::filesystem::directory_iterator end;
    for (; walker != end; walker.increment(failure)) {
        if (failure) {
            return;
        }

        if (!walker->is_directory(failure) || failure) {
            continue;
        }

        const std::string name = walker->path().filename().string();
        if (name.starts_with(".")) {
            continue;
        }

        _candidates.push_back(name);
    }

    std::sort(_candidates.begin(), _candidates.end());
}

std::uint64_t PublishService::NextVersion_(const std::string& identifier) const {
    const auto known = _versions.find(identifier);
    if (known == _versions.end()) {
        return 1;
    }

    return known->second + 1;
}

std::expected<domain::PublishedRelease, domain::PublishError> PublishService::Publish(
    std::string_view folder) {

    if (!_publisher.present) {
        _message = "no signing key";
        return std::unexpected(domain::PublishError::KeyMissing);
    }

    const auto identity = _keyStore.Identity();
    if (!identity.has_value()) {
        _message = "key unreadable";
        return std::unexpected(domain::PublishError::KeyMissing);
    }

    const std::string identifier = identity->fingerprint.ToHex() + "/" + std::string(folder);
    const std::uint64_t version = NextVersion_(identifier);

    const auto manifest = _builder.Build(
        _modsDirectory / folder,
        identity->fingerprint,
        folder,
        version);

    if (!manifest.has_value()) {
        _message = "manifest build failed";
        return std::unexpected(domain::PublishError::ManifestBuildFailed);
    }

    const std::vector<std::uint8_t> payload = _codec.Encode(*manifest);

    const auto sealed = _signer.Seal(payload);
    if (!sealed.has_value()) {
        _message = "sign failed";
        return std::unexpected(domain::PublishError::SignFailed);
    }

    const auto torrent = _torrentBuilder->Build(*manifest, _modsDirectory / folder, *sealed);
    if (!torrent.has_value()) {
        _message = "torrent build failed";
        return std::unexpected(domain::PublishError::ManifestBuildFailed);
    }

    const auto announce = _announcer.Announce(*manifest, *sealed, torrent->infoHash);
    if (!announce.has_value()) {
        _message = "announce failed";
        return std::unexpected(domain::PublishError::AnnounceFailed);
    }

    const std::string manifestDigest = announce->manifestDigest.ToHex();

    const auto stored = _store.Save(manifestDigest, *sealed);
    if (!stored.has_value()) {
        _message = "manifest store failed";
        return std::unexpected(domain::PublishError::StoreFailed);
    }

    const auto accepted = _receiver->Accept(AnnounceCodec::Encode(*announce));
    if (!accepted.has_value()) {
        _message = "announce rejected";
        return std::unexpected(domain::PublishError::AnnounceFailed);
    }

    _versions.insert_or_assign(identifier, version);

    const domain::PublishedRelease release{
        identifier,
        manifest->ModName(),
        version,
        manifest->TotalBytes(),
        manifest->ChunkCount(),
        manifest->Files().size(),
        manifestDigest
    };

    _history.push_back(release);
    _message = "published " + manifest->ModName();

    return release;
}

const std::vector<domain::PublishedRelease>& PublishService::History() const {
    return _history;
}

const std::string& PublishService::LastMessage() const {
    return _message;
}

}
