#include "manager/service/PublishService.h"
#include "manager/announce/AnnounceCodec.h"
#include "manager/identity/ModNameOwnership.h"
#include "manager/text/ServiceText.h"

#include <algorithm>

namespace wgrd::manager {
namespace {
	std::string_view DescribeBuild(const domain::ManifestBuildError failure) {
		switch (failure) {
			case domain::ManifestBuildError::FolderMissing:
				return text::MANIFEST_FOLDER_MISSING;
			case domain::ManifestBuildError::FolderUnreadable:
				return text::MANIFEST_FOLDER_UNREADABLE;
			case domain::ManifestBuildError::FolderEmpty:
				return text::MANIFEST_FOLDER_EMPTY;
			case domain::ManifestBuildError::PathRejected:
				return text::MANIFEST_PATH_REJECTED;
			case domain::ManifestBuildError::FileUnreadable:
				return text::MANIFEST_FILE_UNREADABLE;
			case domain::ManifestBuildError::TooManyChunks:
				return text::MANIFEST_TOO_MANY_CHUNKS;
			case domain::ManifestBuildError::ModNameRejected:
				return text::MANIFEST_NAME_REJECTED;
		}

		return text::MANIFEST_BUILD_FAILED;
	}

	std::string_view DescribeRejection(const domain::AnnounceRejection rejection) {
		switch (rejection) {
			case domain::AnnounceRejection::Malformed:
				return text::ANNOUNCE_MALFORMED;
			case domain::AnnounceRejection::UnknownPublisher:
				return text::ANNOUNCE_UNKNOWN_PUBLISHER;
			case domain::AnnounceRejection::RevokedPublisher:
				return text::ANNOUNCE_REVOKED_PUBLISHER;
			case domain::AnnounceRejection::SignatureInvalid:
				return text::ANNOUNCE_SIGNATURE_INVALID;
			case domain::AnnounceRejection::NotNewer:
				return text::ANNOUNCE_NOT_NEWER;
		}

		return text::ANNOUNCE_REJECTED;
	}

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
	if (ModNameOwnership::Conflicts(folder, identifier, *_catalogue, &_installed)) {
		_message = "Mod name already used";
		return std::unexpected(domain::PublishError::NameConflict);
	}
	const std::uint64_t version = NextVersion_(identity->fingerprint, std::string(folder));

	const auto manifest = _builder.BuildObserved(
		_modsDirectory / folder,
		identity->fingerprint,
		folder,
		version,
		[this](const std::uint64_t processed, const std::uint64_t total) {
			const std::scoped_lock lock(_progressGuard);

			_progress.processedBytes = processed;
			_progress.totalBytes = total;
		}
	);

	if (!manifest.has_value()) {
		_message = std::string(DescribeBuild(manifest.error()));
		return std::unexpected(domain::PublishError::ManifestBuildFailed);
	}

	if (ModNameOwnership::Conflicts(folder, identifier, *_catalogue, &_installed)) {
		_message = "Mod name already used";
		return std::unexpected(domain::PublishError::NameConflict);
	}

	PublishProgress_(domain::PublishPhase::Signing, std::string(text::PUBLISH_SIGNING));

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

	PublishProgress_(domain::PublishPhase::Announcing, std::string(text::PUBLISH_ANNOUNCING));

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
		_message = std::string(DescribeRejection(accepted.error()));
		return std::unexpected(domain::PublishError::AnnounceFailed);
	}

	_versions.insert_or_assign(identifier, version);

	const bool recorded = _installed.Save(domain::InstalledRelease{
			identifier, manifest->ModName(), version, manifestDigest
		}
	);
	if (!recorded) {
		_message = text::MANIFEST_STORE_FAILED;
		return std::unexpected(domain::PublishError::StoreFailed);
	}

	if (_seeding != nullptr) {
		_seeding->AttestContent(
			*manifest,
			_modsDirectory / folder,
			_store.PathFor(manifestDigest)
		);
	}

	const domain::PublishedRelease release{
		identifier, manifest->ModName(), version, manifest->TotalBytes(), manifest->ChunkCount(), manifest->Files().size(), manifestDigest
	};

	_history.push_back(release);
	_message = std::string(text::PUBLISHED_PREFIX) + manifest->ModName();

	return release;
}

}
