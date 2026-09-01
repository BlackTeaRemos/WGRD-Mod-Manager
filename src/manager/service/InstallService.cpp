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
	domain::ISeedingService* seeding
)
	: _modsDirectory(std::move(modsDirectory))
	, _dataDirectory(std::move(dataDirectory))
	, _receiver(&receiver)
	, _store(&store)
	, _authenticator(&authenticator)
	, _codec(&codec)
	, _manifestBuilder(&manifestBuilder)
	, _fetcher(&fetcher)
	, _installed(&installed)
	, _seeding(seeding)
	, _hasher(&hasher)
	, _differ()
	, _installer(hasher)
	, _guard()
	, _progress()
	, _announce()
	, _target()
	, _plan()
	, _awaitingManifest(false)
	, _worker()
	, _busy(false)
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

std::expected<void, domain::InstallStartError> InstallService::Verify(const std::string_view identifier) {
	if (_busy) {
		return std::unexpected(domain::InstallStartError::Busy);
	}

	const auto announce = _receiver->Retained(std::string(identifier));
	if (!announce.has_value()) {
		return std::unexpected(domain::InstallStartError::UnknownIdentifier);
	}

	const auto decoded = DecodeStoredManifest_(announce->manifestDigest.ToHex());
	if (!decoded.has_value()) {
		return std::unexpected(decoded.error());
	}

	bool expected = false;
	if (!_busy.compare_exchange_strong(expected, true)) {
		return std::unexpected(domain::InstallStartError::Busy);
	}

	JoinWorker_();

	{
		const std::scoped_lock lock(_guard);

		_announce = *announce;
		_target = *decoded;
		_progress = domain::InstallProgress{};
		_progress.identifier = _target.Identifier();
		_progress.modName = _target.ModName();
		_progress.version = _target.Version();
		_progress.remoteBytes = _target.TotalBytes();
	}

	Publish_(domain::InstallPhase::Verifying, std::string(text::VERIFY_CHECKING));

	_worker = std::thread([this]() {
			RunVerify_();
		}
	);

	return {};
}

void InstallService::RunVerify_() {
	domain::ModManifest target;

	{
		const std::scoped_lock lock(_guard);
		target = _target;
	}

	const std::filesystem::path modFolder = _modsDirectory / target.ModName();

	std::error_code failure;
	if (!std::filesystem::is_directory(modFolder, failure)) {
		Conclude_(domain::InstallPhase::Failed, std::string(text::VERIFY_FOLDER_MISSING));
		return;
	}

	const InstalledContentAuditor auditor(*_hasher);

	InstalledContentAuditor::Audit audit = auditor.Examine(
		target,
		modFolder,
		[this](const std::uint64_t verifiedBytes) {
			const std::scoped_lock lock(_guard);
			_progress.fetchedBytes = verifiedBytes;
		}
	);

	if (audit.damagedChunks == 0) {
		Conclude_(domain::InstallPhase::Done, std::string(text::VERIFY_INTACT));
		return;
	}

	Publish_(domain::InstallPhase::Planning, std::string(text::VERIFY_REPAIRING));

	StartFetch_(target, domain::InstallPlan(std::move(audit.damagedFiles), {}));
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

void InstallService::RunPlan_() {
	domain::ModManifest target;

	{
		const std::scoped_lock lock(_guard);
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
			0
		);

		if (rebuilt.has_value()) {
			held = *rebuilt;
		}
	}

	StartFetch_(target, _differ.Diff(held, target));
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

void InstallService::Poll() {
	const domain::InstallPhase phase = [this]() {
		const std::scoped_lock lock(_guard);
		return _progress.phase;
	}();

	if (phase != domain::InstallPhase::Fetching) {
		return;
	}

	const domain::FetchStatus status = _fetcher->Fetch();

	{
		const std::scoped_lock lock(_guard);
		_progress.fetchedBytes = status.fetchedBytes;
		_progress.inFlightBytes = status.inFlightBytes;
		_progress.peers = status.peers;
		_progress.pendingWrites = status.pendingWrites;
		_progress.settling = status.settling;

		if (status.settling) {
			_progress.message = std::string(text::INSTALL_SETTLING_WRITES);
		}
	}

	if (status.phase == domain::FetchPhase::Failed) {
		Conclude_(domain::InstallPhase::Failed, std::string(text::INSTALL_FETCH_FAILED));
		return;
	}

	if (status.phase != domain::FetchPhase::Complete) {
		return;
	}

	const bool awaitingManifest = [this]() {
		const std::scoped_lock lock(_guard);
		return _awaitingManifest;
	}();

	if (awaitingManifest) {
		if (!AdoptFetchedManifest_()) {
			_fetcher->Cancel();
			Conclude_(domain::InstallPhase::Failed, std::string(text::INSTALL_MANIFEST_REJECTED));
			return;
		}

		_fetcher->Cancel();

		Publish_(domain::InstallPhase::Planning, std::string(text::INSTALL_COMPARING));

		JoinWorker_();

		_worker = std::thread([this]() {
				RunPlan_();
			}
		);

		return;
	}

	_fetcher->Cancel();

	Publish_(domain::InstallPhase::Installing, std::string(text::INSTALL_MATERIALISING));

	JoinWorker_();

	_worker = std::thread([this]() {
			RunInstall_();
		}
	);
}

void InstallService::Cancel() {
	_fetcher->Cancel();

	JoinWorker_();

	const bool wasBusy = _busy.exchange(false);

	Publish_(domain::InstallPhase::Idle, std::string(text::INSTALL_CANCELLED));

	if (wasBusy) {
		++_settled;
	}
}
}
