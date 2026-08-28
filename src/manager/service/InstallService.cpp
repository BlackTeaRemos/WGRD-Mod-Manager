#include "manager/service/InstallService.h"

#include "manager/install/StagedChunkSource.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <fstream>
#include <iterator>
#include <set>
#include <system_error>
#include <utility>

namespace wgrd::manager {

InstallService::InstallService(
    std::filesystem::path modsDirectory,
    std::filesystem::path dataDirectory,
    const AnnounceReceiver& receiver,
    const ManifestStore& store,
    const domain::IManifestAuthenticator& authenticator,
    const domain::IManifestCodec& codec,
    const domain::IManifestBuilder& manifestBuilder,
    const domain::IContentHasher& hasher,
    domain::IChunkFetcher& fetcher)
    : _modsDirectory(std::move(modsDirectory)),
      _dataDirectory(std::move(dataDirectory)),
      _receiver(&receiver),
      _store(&store),
      _authenticator(&authenticator),
      _codec(&codec),
      _manifestBuilder(&manifestBuilder),
      _fetcher(&fetcher),
      _hasher(&hasher),
      _differ(),
      _installer(hasher),
      _guard(),
      _progress(),
      _announce(),
      _target(),
      _plan(),
      _awaitingManifest(false),
      _worker(),
      _busy(false) {
}

InstallService::~InstallService() {
    JoinWorker_();
}

void InstallService::JoinWorker_() {
    if (_worker.joinable()) {
        _worker.join();
    }
}

std::filesystem::path InstallService::StagingRoot_() const {
    return _dataDirectory / STAGING_FOLDER;
}

void InstallService::Publish_(domain::InstallPhase phase, std::string message) {
    const std::lock_guard<std::mutex> lock(_guard);
    _progress.phase = phase;
    _progress.message = std::move(message);
}

domain::InstallProgress InstallService::Progress() const {
    const std::lock_guard<std::mutex> lock(_guard);
    return _progress;
}

std::expected<void, domain::InstallStartError> InstallService::Start(std::string_view identifier) {
    if (_busy) {
        return std::unexpected(domain::InstallStartError::Busy);
    }

    const auto announce = _receiver->Retained(std::string(identifier));
    if (!announce.has_value()) {
        return std::unexpected(domain::InstallStartError::UnknownIdentifier);
    }

    const bool held = _store->Holds(announce->manifestDigest.ToHex());

    domain::ModManifest manifest;

    if (held) {
        const auto sealed = _store->Load(announce->manifestDigest.ToHex());
        if (!sealed.has_value()) {
            return std::unexpected(domain::InstallStartError::ManifestMissing);
        }

        const auto authenticated = _authenticator->Authenticate(*sealed);
        if (!authenticated.has_value()) {
            return std::unexpected(domain::InstallStartError::ManifestRejected);
        }

        const auto decoded = _codec->Decode(authenticated->payload);
        if (!decoded.has_value()) {
            return std::unexpected(domain::InstallStartError::ManifestRejected);
        }

        manifest = *decoded;
    }

    bool expected = false;
    if (!_busy.compare_exchange_strong(expected, true)) {
        return std::unexpected(domain::InstallStartError::Busy);
    }

    JoinWorker_();

    {
        const std::lock_guard<std::mutex> lock(_guard);

        _announce = *announce;
        _target = manifest;
        _plan = domain::InstallPlan();
        _awaitingManifest = !held;

        _progress = domain::InstallProgress{};
        _progress.identifier = std::string(identifier);
        _progress.modName = announce->modName;
        _progress.version = announce->version;
    }

    if (!held) {
        BeginManifestFetch_();
        return {};
    }

    Publish_(domain::InstallPhase::Planning, "comparing against installed");

    _worker = std::thread([this]() {
        RunPlan_();
    });

    return {};
}

void InstallService::BeginManifestFetch_() {
    domain::SignedAnnounce announce;

    {
        const std::lock_guard<std::mutex> lock(_guard);
        announce = _announce;
    }

    const std::vector<std::string> wanted{std::string(domain::ChunkFileNaming::MANIFEST_FILE)};

    const auto begun = _fetcher->Begin(
        announce.Identifier(),
        announce.torrentInfoHash,
        StagingRoot_(),
        wanted);

    if (!begun.has_value()) {
        Publish_(domain::InstallPhase::Failed, "manifest fetch refused");
        _busy = false;
        return;
    }

    Publish_(domain::InstallPhase::Fetching, "fetching manifest from peers");
}

bool InstallService::AdoptFetchedManifest_() {
    domain::SignedAnnounce announce;

    {
        const std::lock_guard<std::mutex> lock(_guard);
        announce = _announce;
    }

    const std::filesystem::path staged =
        StagingRoot_() / announce.TorrentName() / std::string(domain::ChunkFileNaming::MANIFEST_FILE);

    std::ifstream input(staged, std::ios::binary);
    if (!input) {
        return false;
    }

    std::vector<std::uint8_t> sealed;
    sealed.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());

    if (sealed.empty()) {
        return false;
    }

    std::vector<std::byte> raw;
    raw.reserve(sealed.size());
    for (const std::uint8_t value : sealed) {
        raw.push_back(static_cast<std::byte>(value));
    }

    if (_hasher->Hash(raw) != announce.manifestDigest) {
        return false;
    }

    const auto authenticated = _authenticator->Authenticate(sealed);
    if (!authenticated.has_value()) {
        return false;
    }

    const auto decoded = _codec->Decode(authenticated->payload);
    if (!decoded.has_value()) {
        return false;
    }

    if (decoded->Identifier() != announce.Identifier()
        || decoded->Version() != announce.version) {
        return false;
    }

    if (!_store->Save(announce.manifestDigest.ToHex(), sealed).has_value()) {
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock(_guard);
        _target = *decoded;
        _awaitingManifest = false;
    }

    return true;
}

void InstallService::RunPlan_() {
    domain::ModManifest target;

    {
        const std::lock_guard<std::mutex> lock(_guard);
        target = _target;
    }

    const std::filesystem::path modFolder = _modsDirectory / target.ModName();

    domain::ModManifest held;

    std::error_code failure;
    if (std::filesystem::is_directory(modFolder, failure)) {
        const auto rebuilt = _manifestBuilder->Build(
            modFolder,
            target.Publisher(),
            target.ModName(),
            0);

        if (rebuilt.has_value()) {
            held = *rebuilt;
        }
    }

    const domain::InstallPlan plan = _differ.Diff(held, target);

    std::set<std::string> seen;
    std::vector<std::string> wanted;

    for (const domain::FilePlan& file : plan.Files()) {
        for (const domain::ChunkPlacement& placement : file.placements) {
            if (placement.source != domain::ChunkSourceKind::Remote) {
                continue;
            }

            if (!seen.insert(placement.digest.ToHex()).second) {
                continue;
            }

            wanted.push_back(domain::ChunkFileNaming::FileNameFor(placement.digest));
        }
    }

    {
        const std::lock_guard<std::mutex> lock(_guard);

        _plan = plan;
        _progress.remoteBytes = plan.RemoteBytes();
        _progress.heldBytes = plan.HeldBytes();
        _progress.remoteChunks = plan.RemoteChunkCount();
    }

    if (wanted.empty()) {
        Publish_(domain::InstallPhase::Installing, "materialising from held chunks");
        RunInstall_();
        return;
    }

    const auto begun = _fetcher->Begin(
        target.Identifier(),
        _announce.torrentInfoHash,
        StagingRoot_(),
        wanted);

    if (!begun.has_value()) {
        Publish_(domain::InstallPhase::Failed, "fetch refused");
        _busy = false;
        return;
    }

    Publish_(domain::InstallPhase::Fetching, "fetching missing chunks");
}

void InstallService::RunInstall_() {
    domain::ModManifest target;
    domain::InstallPlan plan;

    {
        const std::lock_guard<std::mutex> lock(_guard);
        target = _target;
        plan = _plan;
    }

    const std::filesystem::path staging = StagingRoot_() / target.TorrentName();
    const std::filesystem::path modFolder = _modsDirectory / target.ModName();

    StagedChunkSource source(staging);

    const auto report = _installer.Apply(plan, modFolder, source);

    if (!report.has_value()) {
        Publish_(domain::InstallPhase::Failed, "install failed");
        _busy = false;
        return;
    }

    std::error_code failure;
    std::filesystem::remove_all(staging, failure);

    {
        const std::lock_guard<std::mutex> lock(_guard);
        _progress.fetchedBytes = report->remoteBytes;
        _progress.heldBytes = report->heldBytes;
    }

    Publish_(domain::InstallPhase::Done, "installed " + target.ModName());
    _busy = false;
}

void InstallService::Poll() {
    domain::InstallPhase phase = domain::InstallPhase::Idle;

    {
        const std::lock_guard<std::mutex> lock(_guard);
        phase = _progress.phase;
    }

    if (phase != domain::InstallPhase::Fetching) {
        return;
    }

    const domain::FetchStatus status = _fetcher->Fetch();

    {
        const std::lock_guard<std::mutex> lock(_guard);
        _progress.fetchedBytes = status.fetchedBytes;
        _progress.peers = status.peers;
    }

    if (status.phase == domain::FetchPhase::Failed) {
        Publish_(domain::InstallPhase::Failed, "fetch failed");
        _busy = false;
        return;
    }

    if (status.phase != domain::FetchPhase::Complete) {
        return;
    }

    bool awaitingManifest = false;

    {
        const std::lock_guard<std::mutex> lock(_guard);
        awaitingManifest = _awaitingManifest;
    }

    if (awaitingManifest) {
        if (!AdoptFetchedManifest_()) {
            Publish_(domain::InstallPhase::Failed, "manifest rejected");
            _fetcher->Cancel();
            _busy = false;
            return;
        }

        _fetcher->Cancel();

        Publish_(domain::InstallPhase::Planning, "comparing against installed");

        JoinWorker_();

        _worker = std::thread([this]() {
            RunPlan_();
        });

        return;
    }

    Publish_(domain::InstallPhase::Installing, "materialising mod folder");

    JoinWorker_();

    _worker = std::thread([this]() {
        RunInstall_();
    });
}

void InstallService::Cancel() {
    _fetcher->Cancel();

    JoinWorker_();

    Publish_(domain::InstallPhase::Idle, "cancelled");
    _busy = false;
}

}
