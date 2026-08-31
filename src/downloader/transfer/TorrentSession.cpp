#include "downloader/transfer/TorrentSession.h"

#include "downloader/announce/AnnounceGossipPlugin.h"
#include "downloader/announce/ControlSwarmTorrent.h"
#include "downloader/storage/InstalledFolderStorage.h"
#include "downloader/torrent/build/VirtualChunkSetTorrent.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace wgrd::downloader {
namespace {
	constexpr std::string_view LOOPBACK_ADDRESS = "127.0.0.1";
	constexpr std::string_view PEER_SENT_BAD_DATA = "peer sent bad data";
	constexpr std::string_view PEER_BANNED_BAD_DATA = "peer banned bad data";

	int AlertMask() {
		constexpr libtorrent::alert_category_t mask =
				libtorrent::alert_category::status |
				libtorrent::alert_category::dht |
				libtorrent::alert_category::peer |
				libtorrent::alert_category::connect |
				libtorrent::alert_category::error;

		return static_cast<int>(static_cast<std::uint32_t>(mask));
	}

	libtorrent::settings_pack BuildSettings(const std::string& listenInterfaces, const bool discovery) {
		libtorrent::settings_pack settings;

		settings.set_bool(libtorrent::settings_pack::enable_dht, discovery);
		settings.set_bool(libtorrent::settings_pack::enable_lsd, discovery);
		settings.set_bool(libtorrent::settings_pack::enable_upnp, discovery);
		settings.set_bool(libtorrent::settings_pack::enable_natpmp, discovery);

		settings.set_str(libtorrent::settings_pack::listen_interfaces, listenInterfaces);
		settings.set_int(libtorrent::settings_pack::alert_mask, AlertMask());

		settings.set_str(libtorrent::settings_pack::user_agent, "wgrd-mod-manager");
		settings.set_str(libtorrent::settings_pack::peer_fingerprint, "-WG0100-");

		settings.set_int(
			libtorrent::settings_pack::max_retry_port_bind,
			TorrentSession::PORT_RETRY_LIMIT
		);

		settings.set_bool(libtorrent::settings_pack::close_redundant_connections, false);
		settings.set_bool(libtorrent::settings_pack::allow_multiple_connections_per_ip, true);

		settings.set_int(
			libtorrent::settings_pack::local_service_announce_interval,
			TorrentSession::LOCAL_ANNOUNCE_SECONDS
		);

		return settings;
	}
}

TorrentSession::TorrentSession(
	std::filesystem::path savePath,
	std::string listenInterfaces,
	const bool discovery
)
	: _locator()
	, _exchange(nullptr)
	, _session(nullptr)
	, _control(nullptr)
	, _savePath(std::move(savePath))
	, _status()
	, _gossip()
	, _manualPeers()
	, _lastNeighbourDial()
	, _lastStatusPoll()
	, _neighbourDials(0)
	, _lastPeerError()
	, _seeded()
	, _entries()
	, _enabled(true)
	, _fetching(nullptr)
	, _wanted()
	, _prioritised(false)
	, _fetch() {
	libtorrent::session_params parameters(BuildSettings(listenInterfaces, discovery));

	parameters.disk_io_constructor = [this](
		libtorrent::io_context& context,
		const libtorrent::settings_interface&,
		libtorrent::counters&
	) -> std::unique_ptr<libtorrent::disk_interface> {
				return std::make_unique<InstalledFolderStorage>(_locator, context, _faults);
			};

	_session = std::make_unique<libtorrent::session>(std::move(parameters));

	_status.running = true;
	_status.listenInterface = listenInterfaces;

	_session->post_dht_stats();
}

TorrentSession::~TorrentSession() = default;

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

	const ControlSwarmTorrent::Built control = ControlSwarmTorrent::Create();

	std::error_code creating;
	std::filesystem::create_directories(dataDirectory, creating);

	const std::filesystem::path payloadPath = dataDirectory / control.name;

	{
		std::ofstream output(payloadPath, std::ios::binary | std::ios::trunc);
		if (!output) {
			return;
		}

		output.write(
			reinterpret_cast<const char*>(control.payload.data()),
			static_cast<std::streamsize>(control.payload.size())
		);
	}

	_locator.RegisterFile(control.name, payloadPath, 0, control.payload.size());

	libtorrent::error_code parsing;
	libtorrent::add_torrent_params parameters = libtorrent::load_torrent_buffer(
		libtorrent::span<const char>(
			control.bencoded.data(),
			static_cast<std::ptrdiff_t>(control.bencoded.size())
		),
		parsing,
		libtorrent::load_torrent_limits{}
	);

	if (parsing || parameters.ti == nullptr) {
		return;
	}

	parameters.save_path = dataDirectory.string();
	parameters.flags &= ~libtorrent::torrent_flags::paused;
	parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

	libtorrent::error_code adding;
	libtorrent::torrent_handle handle = _session->add_torrent(std::move(parameters), adding);

	if (adding || !handle.is_valid()) {
		return;
	}

	_control = std::make_unique<libtorrent::torrent_handle>(std::move(handle));
	_gossip.running = true;
}

const domain::GossipStatus& TorrentSession::Gossip() const {
	return _gossip;
}

void TorrentSession::DialManualPeers_(libtorrent::torrent_handle& handle) const {
	if (!handle.is_valid()) {
		return;
	}

	for (const auto& peer : _manualPeers) {
		libtorrent::error_code parsing;
		const libtorrent::address parsed = libtorrent::make_address(peer.first, parsing);

		if (parsing) {
			continue;
		}

		handle.connect_peer(libtorrent::tcp::endpoint(parsed, peer.second));
	}
}

void TorrentSession::DialNeighbours_(libtorrent::torrent_handle& handle) {
	if (!handle.is_valid()) {
		return;
	}

	const auto ours = _session->listen_port();
	const libtorrent::address loopback = libtorrent::make_address_v4("127.0.0.1");

	for (int offset = 0; offset < PORT_RETRY_LIMIT; ++offset) {
		const auto candidate = static_cast<std::uint16_t>(NEIGHBOUR_BASE_PORT + offset);

		if (candidate == ours) {
			continue;
		}

		handle.connect_peer(libtorrent::tcp::endpoint(loopback, candidate));
		++_neighbourDials;
	}
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

	for (std::size_t index = 0; index < _seeded.size(); ++index) {
		const bool connected = index < _entries.size() && _entries[index].peers > 0;

		if (!connected) {
			DialNeighbours_(*_seeded[index].handle);
		}
	}

	if (_fetching != nullptr) {
		DialManualPeers_(*_fetching);
		DialNeighbours_(*_fetching);
	}
}

bool TorrentSession::AddGossipPeer(const std::string_view address, std::uint16_t port) {
	if (port == 0) {
		return false;
	}

	libtorrent::error_code parsing;
	const libtorrent::address parsed =
			libtorrent::make_address(std::string(address), parsing);

	if (parsing) {
		return false;
	}

	const std::pair<std::string, std::uint16_t> peer{std::string(address), port};

	if (std::ranges::find(_manualPeers, peer) == _manualPeers.end()) {
		_manualPeers.push_back(peer);
	}

	if (_control != nullptr) {
		DialManualPeers_(*_control);
	}

	for (const SeededTorrent& seeded : _seeded) {
		DialManualPeers_(*seeded.handle);
	}

	if (_fetching != nullptr) {
		DialManualPeers_(*_fetching);
	}

	return true;
}

std::uint16_t TorrentSession::ListenPort() const {
	return _session->listen_port();
}

void TorrentSession::ConnectLocalPeer(const std::uint16_t port) {
	if (_fetching == nullptr || !_fetching->is_valid()) {
		return;
	}

	_fetching->connect_peer(libtorrent::tcp::endpoint(
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

	for (const SeededTorrent& seeded : _seeded) {
		if (!seeded.handle->is_valid()) {
			continue;
		}

		if (enabled) {
			seeded.handle->resume();
		} else {
			seeded.handle->pause();
		}
	}
}

bool TorrentSession::AlreadySeeding_(const std::string& identifier) const {
	return std::ranges::any_of(_seeded, [&identifier](const SeededTorrent& seeded) {
			return seeded.identifier == identifier;
		}
	);
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

	if (AlreadySeeding_(identifier)) {
		return std::unexpected(domain::SeedError::AlreadySeeding);
	}

	_locator.Register(manifest, modFolder);

	std::vector<std::uint8_t> sealed;

	{
		std::ifstream input(sealedManifestPath, std::ios::binary);
		if (input) {
			sealed.assign(
				std::istreambuf_iterator<char>(input),
				std::istreambuf_iterator<char>()
			);
		}
	}

	if (sealed.empty()) {
		return std::unexpected(domain::SeedError::TorrentBuildFailed);
	}

	_locator.RegisterFile(
		manifest.TorrentName() + "/" + std::string(domain::ChunkFileNaming::MANIFEST_FILE),
		sealedManifestPath,
		0,
		sealed.size()
	);

	const auto torrent = VirtualChunkSetTorrent::Create(
		manifest,
		modFolder,
		manifest.TorrentName(),
		sealed
	);

	if (!torrent.has_value()) {
		return std::unexpected(domain::SeedError::TorrentBuildFailed);
	}

	libtorrent::error_code parsing;
	libtorrent::add_torrent_params parameters = libtorrent::load_torrent_buffer(
		libtorrent::span<const char>(
			torrent->bencoded.data(),
			static_cast<std::ptrdiff_t>(torrent->bencoded.size())
		),
		parsing,
		libtorrent::load_torrent_limits{}
	);

	if (parsing || parameters.ti == nullptr) {
		return std::unexpected(domain::SeedError::TorrentBuildFailed);
	}

	parameters.save_path = _savePath.string();
	parameters.flags &= ~libtorrent::torrent_flags::paused;
	parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

	libtorrent::error_code adding;
	libtorrent::torrent_handle handle = _session->add_torrent(std::move(parameters), adding);

	if (adding || !handle.is_valid()) {
		return std::unexpected(domain::SeedError::SessionRejected);
	}

	_seeded.push_back(SeededTorrent{
			identifier,
			std::make_shared<libtorrent::torrent_handle>(std::move(handle)),
			manifest,
			modFolder
		}
	);

	DialManualPeers_(*_seeded.back().handle);
	DialNeighbours_(*_seeded.back().handle);

	const domain::SeedEntry entry{
		identifier, manifest.ModName(), manifest.Version(), torrent->payloadBytes, manifest.ChunkCount(), torrent->infoHash, false, 0, 0
	};

	_entries.push_back(entry);

	return entry;
}

bool TorrentSession::StopSeeding(std::string_view identifier) {
	const auto seeded = std::ranges::find_if(_seeded, [&identifier](const SeededTorrent& candidate) {
			return candidate.identifier == identifier;
		}
	);

	if (seeded == _seeded.end()) {
		return false;
	}

	if (_session != nullptr && seeded->handle->is_valid()) {
		_session->remove_torrent(*seeded->handle);
	}

	const auto position = static_cast<std::size_t>(std::distance(_seeded.begin(), seeded));

	_locator.Forget(seeded->manifest, seeded->modFolder);

	_seeded.erase(seeded);

	if (position < _entries.size()) {
		_entries.erase(_entries.begin() + static_cast<std::ptrdiff_t>(position));
	}

	return true;
}

const std::vector<domain::SeedEntry>& TorrentSession::Entries() const {
	return _entries;
}

std::uint64_t TorrentSession::UploadedBytes() const {
	std::uint64_t total = 0;
	for (const domain::SeedEntry& entry : _entries) {
		total += entry.uploadedBytes;
	}
	return total;
}

void TorrentSession::RefreshEntries_() {
	for (std::size_t index = 0; index < _seeded.size() && index < _entries.size(); ++index) {
		const libtorrent::torrent_handle& handle = *_seeded[index].handle;

		if (!handle.is_valid()) {
			continue;
		}

		const libtorrent::torrent_status status = handle.status();

		AccumulateRates_(status.download_payload_rate, status.upload_payload_rate);

		_entries[index].seeding = status.is_seeding;
		_entries[index].peers = static_cast<std::uint32_t>(status.num_peers);
		_entries[index].uploadedBytes = static_cast<std::uint64_t>(status.total_upload);
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
	if (_fetch.Busy()) {
		return std::unexpected(domain::FetchError::Busy);
	}

	if (wantedFiles.empty()) {
		return std::unexpected(domain::FetchError::NothingWanted);
	}

	Cancel();

	_wanted.clear();
	for (const std::string& fileName : wantedFiles) {
		_wanted.insert(fileName);
	}

	for (const domain::ChunkDestination& destination : destinations) {
		_locator.RegisterDestination(
			destination.chunkFileName,
			destination.file,
			destination.offset,
			destination.length
		);
	}

	_locator.SetVerifyExisting(verifyExisting);

	const std::string magnet = "magnet:?xt=urn:btmh:1220" + infoHash.ToHex();

	libtorrent::error_code parsing;
	libtorrent::add_torrent_params parameters = libtorrent::parse_magnet_uri(magnet, parsing);

	if (parsing) {
		return std::unexpected(domain::FetchError::MagnetRejected);
	}

	std::error_code creating;
	std::filesystem::create_directories(stagingFolder, creating);

	parameters.save_path = stagingFolder.string();
	parameters.flags |= libtorrent::torrent_flags::default_dont_download;
	parameters.flags &= ~libtorrent::torrent_flags::paused;
	parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

	libtorrent::error_code adding;
	libtorrent::torrent_handle handle = _session->add_torrent(std::move(parameters), adding);

	if (adding || !handle.is_valid()) {
		return std::unexpected(domain::FetchError::SessionRejected);
	}

	_fetching = std::make_unique<libtorrent::torrent_handle>(std::move(handle));
	_prioritised = false;
	_settledPolls = 0;
	_hashFailures = 0;
	_bannedPeers = 0;
	_lastTransferFailure.clear();

	DialManualPeers_(*_fetching);
	DialNeighbours_(*_fetching);

	_fetch = domain::FetchStatus{};
	_fetch.phase = domain::FetchPhase::Metadata;
	_fetch.identifier = std::move(identifier);
	_fetch.stagingFolder = stagingFolder;

	return {};
}

domain::FetchStatus TorrentSession::Fetch() const {
	return _fetch;
}

void TorrentSession::Cancel() {
	if (_fetching != nullptr && _fetching->is_valid() && _session != nullptr) {
		_session->remove_torrent(*_fetching);
	}

	_fetching.reset();
	_prioritised = false;
	_settledPolls = 0;

	_locator.ClearDestinations();
	_wanted.clear();

	if (_fetch.Busy()) {
		_fetch.phase = domain::FetchPhase::Idle;
	}
}

void TorrentSession::ApplyFetchPriorities_() {
	if (_fetching == nullptr || _prioritised || !_fetching->is_valid()) {
		return;
	}

	const std::shared_ptr<const libtorrent::torrent_info> info = _fetching->torrent_file();
	if (info == nullptr) {
		return;
	}

	const libtorrent::file_storage& files = info->layout();

	std::vector<libtorrent::download_priority_t> priorities;
	priorities.reserve(static_cast<std::size_t>(info->num_files()));

	std::uint64_t wantedBytes = 0;

	for (const libtorrent::file_index_t index : files.file_range()) {
		if (files.pad_file_at(index)) {
			priorities.push_back(libtorrent::dont_download);
			continue;
		}

		const std::string leaf(domain::ChunkFileNaming::LeafOf(files.file_path(index)));

		const bool wanted = _wanted.contains(leaf);

		priorities.push_back(wanted ? libtorrent::default_priority : libtorrent::dont_download);

		if (wanted) {
			wantedBytes += static_cast<std::uint64_t>(files.file_size(index));
		}
	}

	_fetching->prioritize_files(priorities);

	_fetch.wantedBytes = wantedBytes;
	_fetch.phase = domain::FetchPhase::Downloading;
	_prioritised = true;
}

void TorrentSession::RefreshFetch_() {
	if (_fetching == nullptr || !_fetching->is_valid()) {
		return;
	}

	ApplyFetchPriorities_();

	const libtorrent::torrent_status status = _fetching->status();

	AccumulateRates_(status.download_payload_rate, status.upload_payload_rate);

	_fetch.peers = static_cast<std::uint32_t>(status.num_peers);
	_fetch.hashFailures = _hashFailures;
	_fetch.bannedPeers = _bannedPeers;
	_fetch.lastFailure = _lastTransferFailure;
	_fetch.fetchedBytes = static_cast<std::uint64_t>(status.total_wanted_done);

	const std::int64_t received = status.total_payload_download;
	const std::int64_t verified = status.total_wanted_done;

	_fetch.inFlightBytes = received > verified
	                       ? static_cast<std::uint64_t>(received - verified)
	                       : 0;

	const bool wantedComplete = _fetch.wantedBytes > 0
	                            && _fetch.fetchedBytes >= _fetch.wantedBytes
	                            && _fetch.inFlightBytes == 0;

	if (!_prioritised || !(status.is_finished || wantedComplete)) {
		_settledPolls = 0;
		return;
	}

	if (_settledPolls < COMPLETE_CONFIRMATIONS) {
		++_settledPolls;
		return;
	}

	_fetch.phase = domain::FetchPhase::Complete;
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

	for (const libtorrent::alert* const entry : alerts) {
		if (const auto* const stats = libtorrent::alert_cast<libtorrent::dht_stats_alert>(entry)) {
			std::uint64_t nodes = 0;
			for (const libtorrent::dht_routing_bucket& bucket : stats->routing_table) {
				nodes += static_cast<std::uint64_t>(bucket.num_nodes);
			}
			_status.dhtNodes = nodes;
			continue;
		}

		if (libtorrent::alert_cast<libtorrent::hash_failed_alert>(entry) != nullptr) {
			++_hashFailures;
			_lastTransferFailure = std::string(PEER_SENT_BAD_DATA);
			continue;
		}

		if (libtorrent::alert_cast<libtorrent::peer_ban_alert>(entry) != nullptr) {
			++_bannedPeers;
			_lastTransferFailure = std::string(PEER_BANNED_BAD_DATA);
			continue;
		}

		if (const auto* const unreadable =
				libtorrent::alert_cast<libtorrent::file_error_alert>(entry)) {
			_lastTransferFailure = unreadable->error.message();
			continue;
		}

		if (const auto* const broken =
				libtorrent::alert_cast<libtorrent::torrent_error_alert>(entry)) {
			_lastTransferFailure = broken->error.message();
			continue;
		}

		if (const auto* const dropped =
				libtorrent::alert_cast<libtorrent::peer_disconnected_alert>(entry)) {
			const bool refused =
					dropped->error == boost::asio::error::connection_refused ||
					dropped->error == boost::asio::error::timed_out;

			const std::string described = dropped->message();
			if (!refused && described.find(LOOPBACK_ADDRESS) != std::string::npos) {
				_lastPeerError = dropped->error.message();
			}
			continue;
		}

		if (const auto* const failed =
				libtorrent::alert_cast<libtorrent::peer_error_alert>(entry)) {
			_lastPeerError = failed->error.message();
		}
	}

	if (_exchange != nullptr) {
		const bool running = _gossip.running;
		_gossip = _exchange->Snapshot();
		_gossip.running = running;
		_gossip.neighbourDials = _neighbourDials;
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
	RefreshFetch_();

	_status.downloadRate = static_cast<std::uint64_t>(_pendingDownloadRate);
	_status.uploadRate = static_cast<std::uint64_t>(_pendingUploadRate);

	_session->post_dht_stats();
}
}
