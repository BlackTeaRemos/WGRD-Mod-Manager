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
InstallService::InstallService(
	std::filesystem::path modsDirectory,
	std::filesystem::path dataDirectory,
	const AnnounceReceiver& receiver,
	const ManifestStore& store,
	const domain::IManifestAuthenticator& authenticator,
	const domain::IManifestCodec& codec,
	const domain::IManifestBuilder& manifestBuilder,
	const domain::IContentHasher& hasher,
	domain::IChunkFetcher& fetcher,
	const InstalledReleaseStore& installed,
	domain::ISeedingService* seeding,
	domain::IBaselineIndexService* baseline
)
	: _modsDirectory(std::move(modsDirectory))
	, _dataDirectory(std::move(dataDirectory))
	, _receiver(&receiver)
	, _store(&store)
	, _authenticator(&authenticator)
	, _codec(&codec)
	, _fetcher(&fetcher)
	, _installed(&installed)
	, _seeding(seeding)
	, _hasher(&hasher)
	, _differ()
	, _planner(_modsDirectory, manifestBuilder, _differ, baseline)
	, _installer(hasher)
	, _guard()
	, _progress()
	, _announce()
	, _target()
	, _plan()
	, _awaitingManifest(false)
	, _worker()
	, _busy(false)
	, _abandoned(false)
	, _completed(0)
	, _settled(0) {}

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

void InstallService::Publish_(const domain::InstallPhase phase, std::string message) {
	const std::scoped_lock lock(_guard);
	_progress.phase = phase;
	_progress.message = std::move(message);
}

void InstallService::Conclude_(const domain::InstallPhase phase, std::string message) {
	Publish_(phase, std::move(message));
	_busy = false;
	++_settled;
}

void InstallService::WithdrawSeed_(const std::string& identifier) {
	if (_seeding == nullptr) {
		return;
	}

	const bool stopped = _seeding->StopSeeding(identifier);
	(void)stopped;
}

std::string_view InstallService::DescribeRefusal_(
	const domain::FetchError refusal,
	const std::string_view fallback
) {
	if (refusal == domain::FetchError::AlreadyPresent) {
		return text::INSTALL_SEED_CONFLICT;
	}

	return fallback;
}

domain::InstallProgress InstallService::Progress() const {
	const std::scoped_lock lock(_guard);
	return _progress;
}

std::uint64_t InstallService::CompletedInstalls() const {
	return _completed.load();
}

std::uint64_t InstallService::SettledAttempts() const {
	return _settled.load();
}

std::expected<domain::ModManifest, domain::InstallStartError> InstallService::DecodeStoredManifest_(
	const std::string& digestHex
) const {
	const auto sealed = _store->Load(digestHex);
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

	return *decoded;
}

void InstallService::RunPlan_() {
	domain::ModManifest target;

	{
		const std::scoped_lock lock(_guard);
		target = _target;
	}

	const std::optional<domain::InstallPlan> plan = _planner.Plan(
		target,
		[this]() {
			return _abandoned.load();
		},
		[this](const std::string_view message) {
			Publish_(domain::InstallPhase::Planning, std::string(message));
		}
	);

	if (!plan.has_value()) {
		return;
	}

	StartFetch_(target, *plan);
}

void InstallService::StartFetch_(
	const domain::ModManifest& target,
	const domain::InstallPlan& plan
) {
	const std::filesystem::path modFolder = _modsDirectory / target.ModName();

	WithdrawSeed_(target.Identifier());

	const FetchPlanner planner(*_hasher);

	const auto request = planner.Stage(plan, modFolder, ContentInstaller::STAGING_SUFFIX);

	if (!request.has_value()) {
		Conclude_(domain::InstallPhase::Failed, std::string(text::INSTALL_FOLDER_UNWRITABLE));
		return;
	}

	{
		const std::scoped_lock lock(_guard);

		_plan = plan;
		_progress.remoteBytes = plan.RemoteBytes();
		_progress.heldBytes = plan.HeldBytes();
		_progress.remoteChunks = plan.RemoteChunkCount();
	}

	if (request->wantedFiles.empty()) {
		Publish_(domain::InstallPhase::Installing, std::string(text::INSTALL_FROM_HELD));
		RunInstall_();
		return;
	}

	const auto begun = _fetcher->Begin(
		target.Identifier(),
		_announce.torrentInfoHash,
		StagingRoot_(),
		request->wantedFiles,
		request->destinations,
		request->resumed
	);

	if (!begun.has_value()) {
		Conclude_(
			domain::InstallPhase::Failed,
			std::string(DescribeRefusal_(begun.error(), text::INSTALL_FETCH_REFUSED))
		);
		return;
	}

	Publish_(domain::InstallPhase::Fetching, std::string(text::INSTALL_FETCHING_CHUNKS));
}

void InstallService::RunInstall_() {
	domain::ModManifest target;
	domain::InstallPlan plan;

	{
		const std::scoped_lock lock(_guard);
		target = _target;
		plan = _plan;
	}

	const std::filesystem::path staging = StagingRoot_() / target.TorrentName();
	const std::filesystem::path modFolder = _modsDirectory / target.ModName();

	StagedChunkSource source(staging);

	const auto report = _installer.ApplyPlaced(plan, modFolder, source);

	if (!report.has_value()) {
		Conclude_(domain::InstallPhase::Failed, std::string(DescribeInstallError(report.error())));
		return;
	}

	std::error_code failure;
	std::filesystem::remove_all(staging, failure);

	domain::SignedAnnounce announce;

	{
		const std::scoped_lock lock(_guard);
		announce = _announce;
	}

	const bool recorded = _installed->Save(domain::InstalledRelease{
			target.Identifier(), target.ModName(), target.Version(), announce.manifestDigest.ToHex()
		}
	);

	(void)recorded;

	if (_seeding != nullptr) {
		const std::filesystem::path sealed = _store->PathFor(announce.manifestDigest.ToHex());

		_seeding->AttestContent(target, modFolder, sealed);
		_seeding->PrepareSeed(target, modFolder, sealed);
	}

	{
		const std::scoped_lock lock(_guard);
		_progress.fetchedBytes = report->remoteBytes;
		_progress.heldBytes = report->heldBytes;
		_progress.inFlightBytes = 0;
	}

	++_completed;

	Conclude_(domain::InstallPhase::Done, std::string(text::INSTALLED_PREFIX) + target.ModName());
}


}
