#include "manager/service/InstallService.h"

#include "manager/install/ContentInstaller.h"
#include "manager/install/StagedChunkSource.h"
#include "manager/text/ServiceText.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <fstream>
#include <iterator>
#include <set>
#include <system_error>
#include <utility>

namespace wgrd::manager {
namespace {
	std::filesystem::path StagedPathFor_(
		const std::filesystem::path& modFolder,
		const std::string& relativePath
	) {
		const std::filesystem::path target = modFolder / relativePath;

		return std::filesystem::path(target.string() + std::string(ContentInstaller::STAGING_SUFFIX));
	}

	bool SeedFile_(
		const std::filesystem::path& original,
		const std::filesystem::path& target,
		const std::uint64_t size
	) {
		std::error_code failure;
		std::filesystem::create_directories(target.parent_path(), failure);

		if (failure) {
			return false;
		}

		std::filesystem::remove(target, failure);
		failure.clear();

		if (std::filesystem::is_regular_file(original, failure) && !failure) {
			std::filesystem::copy_file(
				original,
				target,
				std::filesystem::copy_options::overwrite_existing,
				failure
			);
		}

		if (failure || !std::filesystem::exists(target, failure)) {
			failure.clear();

			std::ofstream created(target, std::ios::binary | std::ios::trunc);
			if (!created) {
				return false;
			}
		}

		failure.clear();
		std::filesystem::resize_file(target, static_cast<std::uintmax_t>(size), failure);

		return !failure;
	}

	std::string_view DescribeInstall(const InstallError failure) {
		switch (failure) {
			case InstallError::FolderUnwritable:
				return text::INSTALL_FOLDER_UNWRITABLE;
			case InstallError::HeldChunkMissing:
				return text::INSTALL_HELD_MISSING;
			case InstallError::HeldChunkUnreadable:
				return text::INSTALL_HELD_UNREADABLE;
			case InstallError::RemoteChunkUnavailable:
				return text::INSTALL_REMOTE_UNAVAILABLE;
			case InstallError::RemoteChunkCorrupt:
				return text::INSTALL_REMOTE_CORRUPT;
			case InstallError::WriteFailed:
				return text::INSTALL_WRITE_FAILED;
			case InstallError::SwapFailed:
				return text::INSTALL_SWAP_FAILED;
		}

		return text::INSTALL_FAILED;
	}
}

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
	const InstalledReleaseStore& installed
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
	, _completed(0) {}

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

domain::InstallProgress InstallService::Progress() const {
	const std::scoped_lock lock(_guard);
	return _progress;
}

std::uint64_t InstallService::CompletedInstalls() const {
	return _completed.load();
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
		{}
	);

	if (!begun.has_value()) {
		Publish_(domain::InstallPhase::Failed, std::string(text::INSTALL_MANIFEST_REFUSED));
		_busy = false;
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

	std::ifstream input(staged, std::ios::binary);
	if (!input) {
		return false;
	}

	std::vector<std::uint8_t> sealed;
	sealed.assign(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);

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
		const std::scoped_lock lock(_guard);
		_target = *decoded;
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

	const domain::InstallPlan plan = _differ.Diff(held, target);

	std::set<std::string> seen;
	std::vector<std::string> wanted;
	std::vector<domain::ChunkDestination> destinations;

	for (const domain::FilePlan& file : plan.Files()) {
		const std::filesystem::path staged = StagedPathFor_(modFolder, file.path);

		if (!SeedFile_(modFolder / file.path, staged, file.size)) {
			Publish_(domain::InstallPhase::Failed, std::string(text::INSTALL_FOLDER_UNWRITABLE));
			_busy = false;
			return;
		}

		for (const domain::ChunkPlacement& placement : file.placements) {
			if (placement.source != domain::ChunkSourceKind::Remote) {
				continue;
			}

			const std::string chunkFileName = domain::ChunkFileNaming::FileNameFor(placement.digest);

			if (seen.insert(placement.digest.ToHex()).second) {
				wanted.push_back(chunkFileName);
			}

			destinations.push_back(domain::ChunkDestination{
					chunkFileName, staged, placement.targetOffset, placement.length
				}
			);
		}
	}

	{
		const std::scoped_lock lock(_guard);

		_plan = plan;
		_progress.remoteBytes = plan.RemoteBytes();
		_progress.heldBytes = plan.HeldBytes();
		_progress.remoteChunks = plan.RemoteChunkCount();
	}

	if (wanted.empty()) {
		Publish_(domain::InstallPhase::Installing, std::string(text::INSTALL_FROM_HELD));
		RunInstall_();
		return;
	}

	const auto begun = _fetcher->Begin(
		target.Identifier(),
		_announce.torrentInfoHash,
		StagingRoot_(),
		wanted,
		destinations
	);

	if (!begun.has_value()) {
		Publish_(domain::InstallPhase::Failed, std::string(text::INSTALL_FETCH_REFUSED));
		_busy = false;
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
		Publish_(domain::InstallPhase::Failed, std::string(DescribeInstall(report.error())));
		_busy = false;
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

	{
		const std::scoped_lock lock(_guard);
		_progress.fetchedBytes = report->remoteBytes;
		_progress.heldBytes = report->heldBytes;
		_progress.inFlightBytes = 0;
	}

	++_completed;

	Publish_(domain::InstallPhase::Done, std::string(text::INSTALLED_PREFIX) + target.ModName());
	_busy = false;
}

void InstallService::Poll() {
	domain::InstallPhase phase = domain::InstallPhase::Idle;

	{
		const std::scoped_lock lock(_guard);
		phase = _progress.phase;
	}

	if (phase != domain::InstallPhase::Fetching) {
		return;
	}

	const domain::FetchStatus status = _fetcher->Fetch();

	{
		const std::scoped_lock lock(_guard);
		_progress.fetchedBytes = status.fetchedBytes;
		_progress.inFlightBytes = status.inFlightBytes;
		_progress.peers = status.peers;
	}

	if (status.phase == domain::FetchPhase::Failed) {
		Publish_(domain::InstallPhase::Failed, std::string(text::INSTALL_FETCH_FAILED));
		_busy = false;
		return;
	}

	if (status.phase != domain::FetchPhase::Complete) {
		return;
	}

	bool awaitingManifest = false;

	{
		const std::scoped_lock lock(_guard);
		awaitingManifest = _awaitingManifest;
	}

	if (awaitingManifest) {
		if (!AdoptFetchedManifest_()) {
			Publish_(domain::InstallPhase::Failed, std::string(text::INSTALL_MANIFEST_REJECTED));
			_fetcher->Cancel();
			_busy = false;
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

	Publish_(domain::InstallPhase::Idle, std::string(text::INSTALL_CANCELLED));
	_busy = false;
}
}
