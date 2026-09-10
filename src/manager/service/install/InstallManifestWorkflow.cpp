#include "manager/service/InstallService.h"

#include "manager/install/ContentInstaller.h"
#include "manager/install/FetchPlanner.h"
#include "manager/install/FetchedManifestAdopter.h"
#include "manager/install/InstallErrorText.h"
#include "manager/install/InstalledContentAuditor.h"
#include "manager/install/StagedChunkSource.h"
#include "manager/text/ServiceText.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <system_error>
#include <utility>

namespace wgrd::manager {
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
		const auto decoded = DecodeStoredManifest_(announce->manifestDigest.ToHex());
		if (!decoded.has_value()) {
			return std::unexpected(decoded.error());
		}

		manifest = *decoded;
	}

	bool expected = false;
	if (!_busy.compare_exchange_strong(expected, true)) {
		return std::unexpected(domain::InstallStartError::Busy);
	}

	JoinWorker_();

	{
		const std::scoped_lock lock(_guard);

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

	Publish_(domain::InstallPhase::Planning, std::string(text::INSTALL_COMPARING));

	_worker = std::thread([this]() {
			RunPlan_();
		}
	);

	return {};
}

void InstallService::BeginManifestFetch_() {
	domain::SignedAnnounce announce;

	{
		const std::scoped_lock lock(_guard);
		announce = _announce;
	}

	const std::vector<std::string> wanted{std::string(domain::ChunkFileNaming::MANIFEST_FILE)};

	const auto begun = _fetcher->Begin(
		announce.Identifier(),
		announce.torrentInfoHash,
		StagingRoot_(),
		wanted,
		{},
		false
	);

	if (!begun.has_value()) {
		Conclude_(
			domain::InstallPhase::Failed,
			std::string(DescribeRefusal_(begun.error(), text::INSTALL_MANIFEST_REFUSED))
		);
		return;
	}

	Publish_(domain::InstallPhase::Fetching, std::string(text::INSTALL_FETCHING_MANIFEST));
}

bool InstallService::AdoptFetchedManifest_() {
	domain::SignedAnnounce announce;

	{
		const std::scoped_lock lock(_guard);
		announce = _announce;
	}

	const std::filesystem::path staged =
			StagingRoot_() / announce.TorrentName() / std::string(domain::ChunkFileNaming::MANIFEST_FILE);

	const FetchedManifestAdopter adopter(*_hasher, *_authenticator, *_codec, *_store);

	const auto adopted = adopter.Adopt(announce, staged);
	if (!adopted.has_value()) {
		return false;
	}

	{
		const std::scoped_lock lock(_guard);
		_target = *adopted;
		_awaitingManifest = false;
	}

	return true;
}


}
