#include "downloader/transfer/TorrentSession.h"

#include "downloader/announce/AnnounceGossipPlugin.h"
#include "downloader/storage/InstalledFolderStorage.h"
#include "downloader/transfer/AlertRouter.h"
#include "downloader/transfer/ControlSwarmStarter.h"
#include "downloader/transfer/FetchRefresher.h"
#include "downloader/transfer/SeedTorrentAssembler.h"
#include "downloader/transfer/SessionSettings.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

#include <optional>
#include <set>
#include <system_error>
#include <utility>

namespace wgrd::downloader {
TorrentSession::TorrentSession(
	std::filesystem::path savePath,
	std::string listenInterfaces,
	const bool discovery
)
	: _locator()
	, _stamps(savePath / TORRENT_CACHE_FOLDER)
	, _torrents(savePath / TORRENT_CACHE_FOLDER)
	, _exchange(nullptr)
	, _session(nullptr)
	, _control(nullptr)
	, _savePath(std::move(savePath))
	, _status()
	, _gossip()
	, _dialer()
	, _lastNeighbourDial()
	, _lastStatusPoll()
	, _lastPeerError()
	, _lastTransferFailure()
	, _roster()
	, _entriesView()
	, _enabled(true)
	, _fetchState()
	, _present() {
	libtorrent::session_params parameters(SessionSettings::Build(listenInterfaces, discovery));

	parameters.disk_io_constructor = [this](
		libtorrent::io_context& context,
		const libtorrent::settings_interface&,
		libtorrent::counters&
	) -> std::unique_ptr<libtorrent::disk_interface> {
				return std::make_unique<InstalledFolderStorage>(
					_locator,
					context,
					_faults,
					_attestations,
					_backlog
				);
			};

	_session = std::make_unique<libtorrent::session>(std::move(parameters));

	_status.running = true;
	_status.listenInterface = listenInterfaces;

	_session->post_dht_stats();
}

TorrentSession::~TorrentSession() = default;

void TorrentSession::UseTorrentCache(std::filesystem::path folder) {
	_stamps.UseFolder(folder);
	_torrents = TorrentCache(std::move(folder));
}

void TorrentSession::StartGossip(
	domain::IAnnounceCatalogue& catalogue,
	domain::IAnnounceReceiver& receiver,
	const std::filesystem::path& dataDirectory
) {
	if (_session == nullptr || _exchange != nullptr) {
		return;
	}

	_exchange = std::make_unique<AnnounceExchange>(catalogue, receiver);
	_session->add_extension(std::make_shared<AnnounceGossipPlugin>(*_exchange));

	std::optional<PreparedControlTorrent> prepared =
			ControlSwarmStarter::Prepare(_locator, dataDirectory);

	if (!prepared.has_value()) {
		return;
	}

	libtorrent::error_code adding;
	libtorrent::torrent_handle handle =
			_session->add_torrent(std::move(prepared->parameters), adding);

	if (adding || !handle.is_valid()) {
		return;
	}

	if (prepared->hasV2) {
		_present.Record(prepared->infoHash);
	}

	_control = std::make_unique<libtorrent::torrent_handle>(std::move(handle));
	_gossip.running = true;
}

const domain::GossipStatus& TorrentSession::Gossip() const {
	return _gossip;
}

void TorrentSession::DialNeighbours_(libtorrent::torrent_handle& handle) {
	_dialer.DialNeighbours(handle, _session->listen_port());
}

void TorrentSession::DialLoopbackNeighbours_() {
	const auto now = std::chrono::steady_clock::now();
	if (now - _lastNeighbourDial < NEIGHBOUR_DIAL_INTERVAL) {
		return;
	}

	_lastNeighbourDial = now;

	if (_control != nullptr && _controlPeers == 0) {
		DialNeighbours_(*_control);
	}

	for (const SeedHandleView& view : _roster.Handles()) {
		if (view.peers == 0 && view.handle != nullptr) {
			DialNeighbours_(*view.handle);
		}
	}

	std::optional<libtorrent::torrent_handle> active = _fetchState.Active();

	if (active.has_value()) {
		_dialer.DialManual(*active);
		DialNeighbours_(*active);
	}
}

bool TorrentSession::AddGossipPeer(const std::string_view address, std::uint16_t port) {
	if (!_dialer.AddManualPeer(address, port)) {
		return false;
	}

	if (_control != nullptr) {
		_dialer.DialManual(*_control);
	}

	for (const SeedHandleView& view : _roster.Handles()) {
		if (view.handle != nullptr) {
			_dialer.DialManual(*view.handle);
		}
	}

	std::optional<libtorrent::torrent_handle> active = _fetchState.Active();

	if (active.has_value()) {
		_dialer.DialManual(*active);
	}

	return true;
}

std::uint16_t TorrentSession::ListenPort() const {
	return _session->listen_port();
}

void TorrentSession::ConnectLocalPeer(const std::uint16_t port) {
	std::optional<libtorrent::torrent_handle> active = _fetchState.Active();

	if (!active.has_value() || !active->is_valid()) {
		return;
	}

	active->connect_peer(libtorrent::tcp::endpoint(
			libtorrent::make_address_v4("127.0.0.1"),
			port
		)
	);
}

const domain::SwarmStatus& TorrentSession::Status() const {
	return _status;
}

bool TorrentSession::Enabled() const {
	return _enabled;
}

void TorrentSession::SetEnabled(const bool enabled) {
	_enabled = enabled;

	if (_session == nullptr) {
		return;
	}

	for (const SeedHandleView& view : _roster.Handles()) {
		if (view.handle == nullptr || !view.handle->is_valid()) {
			continue;
		}

		if (enabled) {
			view.handle->resume();
		} else {
			view.handle->pause();
		}
	}
}

std::expected<domain::SeedEntry, domain::SeedError> TorrentSession::Announce(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder,
	const std::filesystem::path& sealedManifestPath
) {
	if (!_enabled) {
		return std::unexpected(domain::SeedError::Disabled);
	}

	const std::string identifier = manifest.Identifier();

	if (_roster.Contains(identifier)) {
		return std::unexpected(domain::SeedError::AlreadySeeding);
	}

	_locator.Register(manifest, modFolder);

	const std::string stampKey = sealedManifestPath.stem().string();
	const std::string stamp = ModFolderStamp::Compute(manifest, modFolder);
	const std::optional<std::string> storedStamp = _stamps.Load(stampKey);

	if (storedStamp.has_value() && *storedStamp == stamp) {
		_attestations.Mark(manifest.TorrentName());
	} else {
		_attestations.Forget(manifest.TorrentName());

		const bool stamped = _stamps.Save(stampKey, stamp);
		(void)stamped;
	}

	std::optional<PreparedSeedTorrent> prepared = SeedTorrentAssembler::Prepare(
		_torrents,
		manifest,
		modFolder,
		sealedManifestPath,
		_savePath
	);

	if (!prepared.has_value()) {
		_locator.Forget(manifest, modFolder);
		return std::unexpected(domain::SeedError::TorrentBuildFailed);
	}

	const bool sealedRegistered = _locator.RegisterFile(
		manifest.TorrentName() + "/" + std::string(domain::ChunkFileNaming::MANIFEST_FILE),
		sealedManifestPath,
		0,
		prepared->sealedBytes
	);

	if (!sealedRegistered) {
		_locator.Forget(manifest, modFolder);
		return std::unexpected(domain::SeedError::TorrentBuildFailed);
	}

	libtorrent::error_code adding;
	libtorrent::torrent_handle handle =
			_session->add_torrent(std::move(prepared->parameters), adding);

	if (adding || !handle.is_valid()) {
		_locator.Forget(manifest, modFolder);
		return std::unexpected(domain::SeedError::SessionRejected);
	}

	CapBulkTransfer_(handle);

	auto shared = std::make_shared<libtorrent::torrent_handle>(std::move(handle));

	const domain::SeedEntry entry{
		identifier, manifest.ModName(), manifest.Version(), prepared->payloadBytes, manifest.ChunkCount(), prepared->infoHash, false, 0, 0
	};

	if (!_roster.Add(SeededTorrent{identifier, shared, manifest, modFolder}, entry)) {
		_session->remove_torrent(*shared);
		_locator.Forget(manifest, modFolder);
		return std::unexpected(domain::SeedError::AlreadySeeding);
	}

	_present.Record(prepared->infoHash);

	_dialer.DialManual(*shared);
	DialNeighbours_(*shared);

	return entry;
}

void TorrentSession::AttestContent(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder,
	const std::filesystem::path& sealedManifestPath
) {
	const std::string stampKey = sealedManifestPath.stem().string();
	const std::string stamp = ModFolderStamp::Compute(manifest, modFolder);

	if (!_stamps.Save(stampKey, stamp)) {
		return;
	}

	_attestations.Mark(manifest.TorrentName());
}

bool TorrentSession::StopSeeding(std::string_view identifier) {
	const std::optional<RemovedSeed> removed = _roster.Remove(identifier);

	if (!removed.has_value()) {
		return false;
	}

	if (_session != nullptr && removed->handle != nullptr && removed->handle->is_valid()) {
		_session->remove_torrent(*removed->handle);
	}

	_locator.Forget(removed->manifest, removed->modFolder);
	_attestations.Forget(removed->manifest.TorrentName());

	if (!removed->infoHash.empty()) {
		_present.Forget(removed->infoHash);
	}

	return true;
}

const std::vector<domain::SeedEntry>& TorrentSession::Entries() const {
	_entriesView = _roster.Snapshot();
	return _entriesView;
}

std::uint64_t TorrentSession::UploadedBytes() const {
	return _roster.UploadedBytes();
}

void TorrentSession::RefreshEntries_() {
	for (const SeedHandleView& view : _roster.Handles()) {
		if (view.handle == nullptr || !view.handle->is_valid()) {
			continue;
		}

		const libtorrent::torrent_status status = view.handle->status();

		AccumulateRates_(status.download_payload_rate, status.upload_payload_rate);

		_roster.Update(
			view.identifier,
			status.is_seeding,
			static_cast<std::uint32_t>(status.num_peers),
			static_cast<std::uint64_t>(status.total_upload)
		);
	}
}

std::expected<void, domain::FetchError> TorrentSession::Begin(
	std::string identifier,
	const domain::ChunkDigest& infoHash,
	const std::filesystem::path& stagingFolder,
	const std::vector<std::string>& wantedFiles,
	const std::vector<domain::ChunkDestination>& destinations,
	const bool verifyExisting
) {
	if (_fetchState.Snapshot().Busy()) {
		return std::unexpected(domain::FetchError::Busy);
	}

	if (wantedFiles.empty()) {
		return std::unexpected(domain::FetchError::NothingWanted);
	}

	if (_present.Contains(infoHash.ToHex())) {
		return std::unexpected(domain::FetchError::AlreadyPresent);
	}

	Cancel();

	std::set<std::string> wanted;
	for (const std::string& fileName : wantedFiles) {
		wanted.insert(fileName);
	}

	if (!_fetchState.Reserve(std::move(identifier), stagingFolder, std::move(wanted))) {
		return std::unexpected(domain::FetchError::Busy);
	}

	if (_fetchState.AbandonPendingClear()) {
		_locator.ClearDestinations();
	}

	for (const domain::ChunkDestination& destination : destinations) {
		const bool registered = _locator.RegisterDestination(
			destination.chunkFileName,
			destination.file,
			destination.offset,
			destination.length
		);

		if (!registered) {
			_locator.ClearDestinations();
			_fetchState.Release();
			return std::unexpected(domain::FetchError::DestinationRejected);
		}
	}

	_locator.SetVerifyExisting(verifyExisting);

	const std::string magnet = "magnet:?xt=urn:btmh:1220" + infoHash.ToHex();

	libtorrent::error_code parsing;
	libtorrent::add_torrent_params parameters = libtorrent::parse_magnet_uri(magnet, parsing);

	if (parsing) {
		_locator.ClearDestinations();
		_fetchState.Release();
		return std::unexpected(domain::FetchError::MagnetRejected);
	}

	std::error_code creating;
	std::filesystem::create_directories(stagingFolder, creating);

	parameters.save_path = stagingFolder.string();
	parameters.flags |= libtorrent::torrent_flags::default_dont_download;
	parameters.flags |= libtorrent::torrent_flags::duplicate_is_error;
	parameters.flags &= ~libtorrent::torrent_flags::paused;
	parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

	libtorrent::error_code adding;
	libtorrent::torrent_handle handle = _session->add_torrent(std::move(parameters), adding);

	if (adding || !handle.is_valid()) {
		_locator.ClearDestinations();
		_fetchState.Release();
		return std::unexpected(domain::FetchError::SessionRejected);
	}

	CapBulkTransfer_(handle);

	_fetchState.Adopt(handle);

	_dialer.DialManual(handle);
	DialNeighbours_(handle);

	return {};
}

domain::FetchStatus TorrentSession::Fetch() const {
	return _fetchState.Snapshot();
}

void TorrentSession::Cancel() {
	const bool drained = _backlog.AwaitDrain(WRITE_DRAIN_TIMEOUT);
	(void)drained;

	const FetchRetirement retirement = _fetchState.Retire();

	if (retirement.toRemove.has_value() && _session != nullptr) {
		_session->remove_torrent(*retirement.toRemove);
	}

	if (retirement.clearNow) {
		_locator.ClearDestinations();
	}
}

std::size_t TorrentSession::RegisteredChunkFiles() const {
	return _locator.Count();
}

std::size_t TorrentSession::RegisteredDestinations() const {
	return _locator.DestinationCount();
}

void TorrentSession::CapBulkTransfer_(libtorrent::torrent_handle& handle) {
	if (!handle.is_valid()) {
		return;
	}

	handle.set_upload_limit(BULK_RATE_CEILING_BYTES_PER_SECOND);
	handle.set_download_limit(BULK_RATE_CEILING_BYTES_PER_SECOND);
}

int TorrentSession::SeededUploadLimit(const std::string_view identifier) const {
	const std::shared_ptr<libtorrent::torrent_handle> handle = _roster.HandleFor(identifier);
	return handle != nullptr && handle->is_valid() ? handle->upload_limit() : -1;
}

int TorrentSession::SeededDownloadLimit(const std::string_view identifier) const {
	const std::shared_ptr<libtorrent::torrent_handle> handle = _roster.HandleFor(identifier);
	return handle != nullptr && handle->is_valid() ? handle->download_limit() : -1;
}

int TorrentSession::FetchUploadLimit() const {
	const std::optional<libtorrent::torrent_handle> active = _fetchState.Active();
	return active.has_value() && active->is_valid() ? active->upload_limit() : -1;
}

int TorrentSession::FetchDownloadLimit() const {
	const std::optional<libtorrent::torrent_handle> active = _fetchState.Active();
	return active.has_value() && active->is_valid() ? active->download_limit() : -1;
}

int TorrentSession::ControlUploadLimit() const {
	return _control != nullptr && _control->is_valid() ? _control->upload_limit() : -1;
}

int TorrentSession::ControlDownloadLimit() const {
	return _control != nullptr && _control->is_valid() ? _control->download_limit() : -1;
}

void TorrentSession::AccumulateRates_(const int downloadRate, const int uploadRate) {
	_pendingDownloadRate += downloadRate < 0 ? 0 : downloadRate;
	_pendingUploadRate += uploadRate < 0 ? 0 : uploadRate;
}

void TorrentSession::Poll() {
	if (_session == nullptr) {
		return;
	}

	std::vector<libtorrent::alert*> alerts;
	_session->pop_alerts(&alerts);

	const AlertOutcome outcome = AlertRouter::Route(alerts, _fetchState);

	if (outcome.dhtNodes.has_value()) {
		_status.dhtNodes = *outcome.dhtNodes;
	}

	if (outcome.clearDestinations) {
		_locator.ClearDestinations();
	}

	if (outcome.lastPeerError.has_value()) {
		_lastPeerError = *outcome.lastPeerError;
	}

	if (outcome.lastTransferFailure.has_value()) {
		_lastTransferFailure = *outcome.lastTransferFailure;
	}

	if (_exchange != nullptr) {
		const bool running = _gossip.running;
		_gossip = _exchange->Snapshot();
		_gossip.running = running;
		_gossip.neighbourDials = _dialer.NeighbourDials();
		_gossip.lastPeerError = _lastPeerError;
		_gossip.lastFailure = _lastTransferFailure;
		_gossip.readFailures = _faults.ReadFailures();
		_gossip.writeFailures = _faults.WriteFailures();
		_gossip.controlValid = _control != nullptr && _control->is_valid();
		_gossip.controlPeers = _controlPeers;
		_gossip.controlState = _controlState;
	}

	const auto now = std::chrono::steady_clock::now();
	if (now - _lastStatusPoll < STATUS_POLL_INTERVAL) {
		return;
	}

	_lastStatusPoll = now;

	_pendingDownloadRate = 0;
	_pendingUploadRate = 0;

	_status.dhtRunning = _session->is_dht_running();
	_status.listenPort = static_cast<std::uint32_t>(_session->listen_port());

	if (_exchange != nullptr) {
		DialLoopbackNeighbours_();

		if (_gossip.controlValid) {
			const libtorrent::torrent_status controlStatus = _control->status();

			AccumulateRates_(controlStatus.download_payload_rate, controlStatus.upload_payload_rate);

			_controlPeers = static_cast<std::uint32_t>(controlStatus.num_peers);
			_controlState = static_cast<std::uint32_t>(controlStatus.state);

			_gossip.controlPeers = _controlPeers;
			_gossip.controlState = _controlState;
		}
	}

	RefreshEntries_();

	const std::optional<FetchRates> fetchRates =
			FetchRefresher::Refresh(_fetchState, _backlog.Pending() == 0);
	if (fetchRates.has_value()) {
		AccumulateRates_(fetchRates->downloadRate, fetchRates->uploadRate);
	}

	_status.downloadRate = static_cast<std::uint64_t>(_pendingDownloadRate);
	_status.uploadRate = static_cast<std::uint64_t>(_pendingUploadRate);

	_session->post_dht_stats();
}
}
