#include "downloader/transfer/SwarmNode.h"

#include "downloader/storage/InstalledFolderStorage.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace wgrd::downloader {
namespace {
	constexpr char LOOPBACK_INTERFACE[] = "127.0.0.1:0";
	constexpr std::string_view LISTEN_BIND_FAILED = "listen bind failed";
	constexpr std::chrono::milliseconds POLL_INTERVAL{25};
	constexpr std::chrono::milliseconds LISTEN_TIMEOUT{8000};
	constexpr int LISTEN_ATTEMPTS = 3;
	constexpr std::chrono::milliseconds REDIAL_INTERVAL{2000};

	int LocalAlertMask() {
		constexpr libtorrent::alert_category_t mask =
				libtorrent::alert_category::status |
				libtorrent::alert_category::storage |
				libtorrent::alert_category::peer |
				libtorrent::alert_category::connect |
				libtorrent::alert_category::piece_progress |
				libtorrent::alert_category::error;

		return static_cast<int>(static_cast<std::uint32_t>(mask));
	}

	libtorrent::settings_pack BuildLocalSettings() {
		libtorrent::settings_pack settings;

		settings.set_bool(libtorrent::settings_pack::enable_dht, false);
		settings.set_bool(libtorrent::settings_pack::enable_lsd, false);
		settings.set_bool(libtorrent::settings_pack::enable_upnp, false);
		settings.set_bool(libtorrent::settings_pack::enable_natpmp, false);

		settings.set_str(libtorrent::settings_pack::listen_interfaces, LOOPBACK_INTERFACE);
		settings.set_int(libtorrent::settings_pack::alert_mask, LocalAlertMask());

		settings.set_str(libtorrent::settings_pack::user_agent, "wgrd-mod-manager");
		settings.set_str(libtorrent::settings_pack::peer_fingerprint, "-WG0100-");

		settings.set_bool(libtorrent::settings_pack::allow_multiple_connections_per_ip, true);

		return settings;
	}
}

SwarmNode::SwarmNode(std::filesystem::path savePath, const ChunkLocator& locator)
	: _faults()
	, _session(nullptr)
	, _handle(nullptr)
	, _savePath(std::move(savePath))
	, _listenPort(0)
	, _cacheFlushed(false)
	, _listenFailure() {
	libtorrent::session_params parameters(BuildLocalSettings());

	parameters.disk_io_constructor = [&locator, this](
		libtorrent::io_context& context,
		const libtorrent::settings_interface&,
		libtorrent::counters&
	) -> std::unique_ptr<libtorrent::disk_interface> {
				return std::make_unique<InstalledFolderStorage>(
					locator,
					context,
					_faults,
					_attestations,
					_backlog
				);
			};

	_session = std::make_unique<libtorrent::session>(std::move(parameters));

	AwaitListener_();
}

SwarmNode::SwarmNode(std::filesystem::path savePath)
	: _faults()
	, _session(std::make_unique<libtorrent::session>(BuildLocalSettings()))
	, _handle(nullptr)
	, _savePath(std::move(savePath))
	, _listenPort(0)
	, _cacheFlushed(false)
	, _listenFailure() {
	AwaitListener_();
}

void SwarmNode::AwaitListener_() {
	for (int attempt = 0; attempt < LISTEN_ATTEMPTS && _listenPort == 0; ++attempt) {
		if (attempt > 0) {
			_session->reopen_network_sockets(libtorrent::session_handle::reopen_map_ports);
		}

		const auto deadline = std::chrono::steady_clock::now() + LISTEN_TIMEOUT;

		while (std::chrono::steady_clock::now() < deadline) {
			DrainAlerts_();

			if (_listenPort != 0) {
				return;
			}

			const int port = _session->listen_port();
			if (port > 0) {
				_listenPort = static_cast<std::uint16_t>(port);
				return;
			}

			std::this_thread::sleep_for(POLL_INTERVAL);
		}
	}
}

SwarmNode::~SwarmNode() = default;

std::uint16_t SwarmNode::ListenPort() const {
	return _listenPort;
}

void SwarmNode::DrainAlerts_() {
	std::vector<libtorrent::alert*> alerts;
	_session->pop_alerts(&alerts);

	for (const libtorrent::alert* const entry : alerts) {
		if (_messages.size() < 200) {
			_messages.push_back(entry->message());
		}

		if (libtorrent::alert_cast<libtorrent::cache_flushed_alert>(entry) != nullptr) {
			_cacheFlushed = true;
			continue;
		}

		if (const auto* const refused =
				libtorrent::alert_cast<libtorrent::listen_failed_alert>(entry)) {
			_listenFailure = std::string(LISTEN_BIND_FAILED) + " " + refused->error.message();
			continue;
		}

		if (const auto* const listening =
				libtorrent::alert_cast<libtorrent::listen_succeeded_alert>(entry)) {
			if (_listenPort == 0 && listening->port != 0) {
				_listenPort = static_cast<std::uint16_t>(listening->port);
			}
		}
	}
}

std::expected<void, SwarmNodeError> SwarmNode::Load(
	const std::span<const char> torrentBytes,
	const bool seeding
) {
	if (_handle != nullptr) {
		return std::unexpected(SwarmNodeError::AlreadyLoaded);
	}

	if (_listenPort == 0) {
		return std::unexpected(SwarmNodeError::NotListening);
	}

	libtorrent::error_code parsing;
	libtorrent::add_torrent_params parameters = libtorrent::load_torrent_buffer(
		libtorrent::span<const char>(torrentBytes.data(), static_cast<std::ptrdiff_t>(torrentBytes.size())),
		parsing,
		libtorrent::load_torrent_limits{}
	);

	if (parsing || parameters.ti == nullptr) {
		return std::unexpected(SwarmNodeError::TorrentRejected);
	}

	parameters.save_path = _savePath.string();

	if (seeding) {
		parameters.flags |= libtorrent::torrent_flags::seed_mode;
	}

	parameters.flags &= ~libtorrent::torrent_flags::paused;
	parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

	libtorrent::error_code adding;
	libtorrent::torrent_handle handle = _session->add_torrent(std::move(parameters), adding);
	if (adding || !handle.is_valid()) {
		return std::unexpected(SwarmNodeError::TorrentRejected);
	}

	_handle = std::make_unique<libtorrent::torrent_handle>(std::move(handle));

	return {};
}

void SwarmNode::ConnectLoopbackPeer(const std::uint16_t port) {
	if (_handle == nullptr) {
		return;
	}

	const libtorrent::tcp::endpoint peer(
		libtorrent::make_address_v4("127.0.0.1"),
		port
	);

	_handle->connect_peer(peer);
}

void SwarmNode::Pump() {
	DrainAlerts_();
}

const std::vector<std::string>& SwarmNode::Messages() const {
	return _messages;
}

const std::string& SwarmNode::ListenFailure() const {
	return _listenFailure;
}

float SwarmNode::Progress() const {
	if (_handle == nullptr) {
		return 0.0f;
	}

	return _handle->status().progress;
}

bool SwarmNode::WaitForCompletion(
	const std::chrono::milliseconds timeout,
	SwarmNode* companion,
	const std::uint16_t redialPort
) {
	if (_handle == nullptr) {
		return false;
	}

	const auto deadline = std::chrono::steady_clock::now() + timeout;

	bool finished = false;
	auto nextDial = std::chrono::steady_clock::now() + REDIAL_INTERVAL;

	while (std::chrono::steady_clock::now() < deadline) {
		DrainAlerts_();
		if (companion != nullptr) {
			companion->Pump();
		}

		if (redialPort != 0 && std::chrono::steady_clock::now() >= nextDial) {
			ConnectLoopbackPeer(redialPort);
			nextDial = std::chrono::steady_clock::now() + REDIAL_INTERVAL;
		}

		const libtorrent::torrent_status status = _handle->status();
		if (status.is_seeding || status.is_finished) {
			finished = true;
			break;
		}

		std::this_thread::sleep_for(POLL_INTERVAL);
	}

	if (!finished) {
		return false;
	}

	_cacheFlushed = false;
	_handle->flush_cache();

	while (std::chrono::steady_clock::now() < deadline) {
		DrainAlerts_();

		if (_cacheFlushed) {
			return true;
		}

		std::this_thread::sleep_for(POLL_INTERVAL);
	}

	return false;
}
}
