#include "manager/service/PublishService.h"

#include "manager/announce/AnnounceCodec.h"
#include "manager/publish/PublisherKeyExporter.h"
#include "manager/publish/PublisherRevocationExporter.h"
#include "manager/text/ServiceText.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/RevocationSignable.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <system_error>
#include <utility>
#include <vector>

namespace wgrd::manager {
namespace {
	constexpr std::string_view REGISTRY_FOLDER = "registry";
	constexpr std::string_view MANIFEST_FOLDER = "manifests";

	domain::PublishError Translate(const domain::SigningKeyStoreError failure) {
		switch (failure) {
			case domain::SigningKeyStoreError::AlreadyPresent:
				return domain::PublishError::KeyAlreadyPresent;
			case domain::SigningKeyStoreError::Absent:
				return domain::PublishError::KeyMissing;
			case domain::SigningKeyStoreError::Locked:
				return domain::PublishError::KeyLocked;
			case domain::SigningKeyStoreError::PassphraseRejected:
				return domain::PublishError::PassphraseRejected;
			case domain::SigningKeyStoreError::PassphraseTooShort:
				return domain::PublishError::PassphraseTooShort;
			case domain::SigningKeyStoreError::PublisherRejected:
				return domain::PublishError::PublisherNameRejected;
			case domain::SigningKeyStoreError::Unreadable:
			case domain::SigningKeyStoreError::Unwritable:
			case domain::SigningKeyStoreError::Corrupt:
			case domain::SigningKeyStoreError::GenerationFailed:
				return domain::PublishError::KeyCreateFailed;
		}

		return domain::PublishError::KeyCreateFailed;
	}

	std::string_view Describe(const domain::SigningKeyStoreError failure) {
		switch (failure) {
			case domain::SigningKeyStoreError::AlreadyPresent:
				return text::KEY_FILE_PRESENT;
			case domain::SigningKeyStoreError::Absent:
				return text::KEY_FILE_MISSING;
			case domain::SigningKeyStoreError::Locked:
				return text::KEY_LOCKED;
			case domain::SigningKeyStoreError::PassphraseRejected:
				return text::PASSPHRASE_REJECTED;
			case domain::SigningKeyStoreError::PassphraseTooShort:
				return text::PASSPHRASE_TOO_SHORT;
			case domain::SigningKeyStoreError::PublisherRejected:
				return text::PUBLISHER_NAME_REJECTED;
			case domain::SigningKeyStoreError::Unreadable:
				return text::KEY_FILE_UNREADABLE;
			case domain::SigningKeyStoreError::Unwritable:
				return text::KEY_FILE_UNWRITABLE;
			case domain::SigningKeyStoreError::Corrupt:
				return text::KEY_FILE_CORRUPT;
			case domain::SigningKeyStoreError::GenerationFailed:
				return text::KEY_GENERATION_FAILED;
		}

		return text::KEY_FAILED;
	}
}

PublishService::PublishService(
	std::filesystem::path modsDirectory,
	std::filesystem::path dataDirectory,
	const domain::IContentChunker& chunker,
	const domain::IContentHasher& hasher,
	const domain::IPayloadPathPolicy& pathPolicy,
	const domain::IChunkSetTorrentBuilder& torrentBuilder,
	domain::IAnnounceReceiver& receiver,
	const domain::IAnnounceCatalogue& catalogue,
	domain::IKeyRegistry& registry
)
	: _modsDirectory(std::move(modsDirectory))
	, _dataDirectory(std::move(dataDirectory))
	, _hasher(&hasher)
	, _torrentBuilder(&torrentBuilder)
	, _receiver(&receiver)
	, _catalogue(&catalogue)
	, _registry(&registry)
	, _keyStore()
	, _builder(chunker, hasher, pathPolicy)
	, _codec(pathPolicy)
	, _signer(_keyStore)
	, _announcer(_keyStore, hasher)
	, _store(_dataDirectory / MANIFEST_FOLDER)
	, _publisher{false, {}, {}}
	, _keyPath()
	, _candidates()
	, _history()
	, _versions()
	, _message() {
	RefreshCandidates();
}

PublishService::~PublishService() = default;

std::string PublishService::Today_() {
	const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
	return std::format("{:%Y-%m-%d}", today);
}

std::filesystem::path PublishService::LocalRegistryFolder_() const {
	return _dataDirectory / REGISTRY_FOLDER / DirectoryKeyRegistry::KEYS_FOLDER;
}

bool PublishService::TrustLocally_(const domain::PublisherIdentity& identity) {
	const auto exported = PublisherKeyExporter::Export(
		identity,
		_keyStore.PublisherName(),
		Today_(),
		LocalRegistryFolder_(),
		true
	);

	if (!exported.has_value()) {
		return false;
	}

	_registry->Reload();

	return true;
}

std::filesystem::path PublishService::SubmissionFolder_(
	const std::filesystem::path& keyPath,
	const std::string_view leaf
) {
	const std::filesystem::path parent = keyPath.parent_path();

	return (parent.empty() ? std::filesystem::current_path() : parent) / leaf;
}

std::filesystem::path PublishService::WriteRegistration_(
	const domain::PublisherIdentity& identity,
	const std::filesystem::path& keyPath
) {
	const auto exported = PublisherKeyExporter::Export(
		identity,
		_keyStore.PublisherName(),
		Today_(),
		SubmissionFolder_(keyPath, DirectoryKeyRegistry::KEYS_FOLDER),
		true
	);

	if (!exported.has_value()) {
		return {};
	}

	return *exported;
}

std::filesystem::path PublishService::WriteRevocation_(
	const domain::PublisherIdentity& identity,
	const std::filesystem::path& keyPath
) {
	const std::string revokedAt = Today_();

	const std::vector<std::uint8_t> signable = RevocationSignable::Bytes(
		identity.fingerprint.ToHex(),
		revokedAt,
		PublisherRevocationExporter::DEFAULT_REASON
	);

	const auto signature = _keyStore.Sign(signable);
	if (!signature.has_value()) {
		return {};
	}

	const auto exported = PublisherRevocationExporter::Export(
		identity,
		*signature,
		revokedAt,
		PublisherRevocationExporter::DEFAULT_REASON,
		SubmissionFolder_(keyPath, DirectoryKeyRegistry::REVOKED_FOLDER),
		true
	);

	if (!exported.has_value()) {
		return {};
	}

	return *exported;
}

const domain::PublisherState& PublishService::Publisher() const {
	return _publisher;
}

const std::filesystem::path& PublishService::KeyPath() const {
	return _keyPath;
}

const ManifestStore& PublishService::Store() const {
	return _store;
}

std::expected<domain::PublisherState, domain::PublishError> PublishService::CreateKey(
	const std::string_view publisherName,
	const std::filesystem::path& keyPath,
	const std::string_view passphrase
) {
	const auto identity = _keyStore.Create(keyPath, publisherName, passphrase);
	if (!identity.has_value()) {
		_message = std::string(Describe(identity.error()));
		return std::unexpected(Translate(identity.error()));
	}

	_keyPath = keyPath;
	_publisher = domain::PublisherState{
		true, identity->fingerprint.ToHex(), _keyStore.PublisherName()
	};

	const std::filesystem::path registration = WriteRegistration_(*identity, keyPath);
	const std::filesystem::path revocation = WriteRevocation_(*identity, keyPath);

	const bool trusted = TrustLocally_(*identity);

	if (!trusted) {
		_message = text::KEY_CREATED_UNTRUSTED;
	} else if (registration.empty()) {
		_message = text::KEY_CREATED_NO_REGISTRATION;
	} else if (revocation.empty()) {
		_message = text::KEY_CREATED_NO_REVOCATION;
	} else {
		_message = text::KEY_CREATED;
	}

	return _publisher;
}

std::expected<domain::PublisherState, domain::PublishError> PublishService::UnlockKey(
	const std::filesystem::path& keyPath,
	const std::string_view passphrase
) {
	const auto identity = _keyStore.Unlock(keyPath, passphrase);
	if (!identity.has_value()) {
		_message = std::string(Describe(identity.error()));
		return std::unexpected(Translate(identity.error()));
	}

	bool trusted = true;
	if (!_registry->Find(identity->fingerprint).has_value()) {
		trusted = TrustLocally_(*identity);
	}

	_keyPath = keyPath;
	_publisher = domain::PublisherState{
		true, identity->fingerprint.ToHex(), _keyStore.PublisherName()
	};

	_message = trusted ? text::KEY_UNLOCKED : text::KEY_UNLOCKED_UNTRUSTED;

	return _publisher;
}

void PublishService::LockKey() {
	_keyStore.Lock();

	_keyPath.clear();
	_publisher = domain::PublisherState{false, {}, {}};
	_message = text::KEY_LOCKED;
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

	std::ranges::sort(_candidates);
}

std::uint64_t PublishService::NextVersion_(
	const domain::PublisherFingerprint& publisher,
	const std::string& modName
) const {
	std::uint64_t highest = 0;

	const auto known = _versions.find(publisher.ToHex() + "/" + modName);
	if (known != _versions.end()) {
		highest = known->second;
	}

	for (const domain::AnnounceSummary& summary : _catalogue->Summaries()) {
		if (summary.publisher == publisher && summary.modName == modName) {
			highest = std::max(highest, summary.version);
		}
	}

	return highest + 1;
}

std::expected<domain::PublishedRelease, domain::PublishError> PublishService::Publish(
	std::string_view folder
) {
	const auto identity = _keyStore.Identity();
	if (!identity.has_value()) {
		_message = text::NO_UNLOCKED_KEY;
		return std::unexpected(domain::PublishError::KeyLocked);
	}

	const std::string identifier = identity->fingerprint.ToHex() + "/" + std::string(folder);
	const std::uint64_t version = NextVersion_(identity->fingerprint, std::string(folder));

	const auto manifest = _builder.Build(
		_modsDirectory / folder,
		identity->fingerprint,
		folder,
		version
	);

	if (!manifest.has_value()) {
		_message = text::MANIFEST_BUILD_FAILED;
		return std::unexpected(domain::PublishError::ManifestBuildFailed);
	}

	const std::vector<std::uint8_t> payload = _codec.Encode(*manifest);

	const auto sealed = _signer.Seal(payload);
	if (!sealed.has_value()) {
		_message = text::SIGN_FAILED;
		return std::unexpected(domain::PublishError::SignFailed);
	}

	const auto torrent = _torrentBuilder->Build(*manifest, _modsDirectory / folder, *sealed);
	if (!torrent.has_value()) {
		_message = text::TORRENT_BUILD_FAILED;
		return std::unexpected(domain::PublishError::ManifestBuildFailed);
	}

	const auto announce = _announcer.Announce(*manifest, *sealed, torrent->infoHash);
	if (!announce.has_value()) {
		_message = text::ANNOUNCE_FAILED;
		return std::unexpected(domain::PublishError::AnnounceFailed);
	}

	const std::string manifestDigest = announce->manifestDigest.ToHex();

	const auto stored = _store.Save(manifestDigest, *sealed);
	if (!stored.has_value()) {
		_message = text::MANIFEST_STORE_FAILED;
		return std::unexpected(domain::PublishError::StoreFailed);
	}

	const auto accepted = _receiver->Accept(AnnounceCodec::Encode(*announce));
	if (!accepted.has_value()) {
		_message = text::ANNOUNCE_REJECTED;
		return std::unexpected(domain::PublishError::AnnounceFailed);
	}

	_versions.insert_or_assign(identifier, version);

	const domain::PublishedRelease release{
		identifier, manifest->ModName(), version, manifest->TotalBytes(), manifest->ChunkCount(), manifest->Files().size(), manifestDigest
	};

	_history.push_back(release);
	_message = std::string(text::PUBLISHED_PREFIX) + manifest->ModName();

	return release;
}

const std::vector<domain::PublishedRelease>& PublishService::History() const {
	return _history;
}

const std::string& PublishService::LastMessage() const {
	return _message;
}
}
