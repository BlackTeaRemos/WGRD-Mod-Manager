#include "downloader/transfer/SessionSettings.h"

#include "downloader/transfer/TorrentSession.h"

#include <libtorrent/alert.hpp>

#include <cstdint>

namespace wgrd::downloader {
int SessionSettings::AlertMask_() {
	constexpr libtorrent::alert_category_t mask =
			libtorrent::alert_category::status |
			libtorrent::alert_category::dht |
			libtorrent::alert_category::peer |
			libtorrent::alert_category::connect |
			libtorrent::alert_category::tracker |
			libtorrent::alert_category::error;

	return static_cast<int>(static_cast<std::uint32_t>(mask));
}

libtorrent::settings_pack SessionSettings::Build(
	const std::string& listenInterfaces,
	const bool discovery
) {
	libtorrent::settings_pack settings;

	settings.set_bool(libtorrent::settings_pack::enable_dht, discovery);
	settings.set_bool(libtorrent::settings_pack::enable_lsd, discovery);
	settings.set_bool(libtorrent::settings_pack::enable_upnp, discovery);
	settings.set_bool(libtorrent::settings_pack::enable_natpmp, discovery);

	settings.set_str(libtorrent::settings_pack::listen_interfaces, listenInterfaces);
	settings.set_int(libtorrent::settings_pack::alert_mask, AlertMask_());

	settings.set_str(libtorrent::settings_pack::user_agent, "wgrd-mod-manager");
	settings.set_str(libtorrent::settings_pack::peer_fingerprint, "-WG0100-");

	settings.set_int(
		libtorrent::settings_pack::max_retry_port_bind,
		TorrentSession::PORT_RETRY_LIMIT
	);

	settings.set_bool(libtorrent::settings_pack::close_redundant_connections, false);
	settings.set_bool(libtorrent::settings_pack::allow_multiple_connections_per_ip, true);

	settings.set_bool(libtorrent::settings_pack::announce_to_all_trackers, true);
	settings.set_bool(libtorrent::settings_pack::announce_to_all_tiers, true);

	settings.set_int(
		libtorrent::settings_pack::local_service_announce_interval,
		TorrentSession::LOCAL_ANNOUNCE_SECONDS
	);

	return settings;
}
}
