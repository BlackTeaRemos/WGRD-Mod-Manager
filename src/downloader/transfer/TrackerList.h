#pragma once

#include <libtorrent/add_torrent_params.hpp>

#include <array>
#include <string_view>

namespace wgrd::downloader {
class TrackerList {
public:
	static constexpr std::array<std::string_view, 10> ANNOUNCE_URLS = {
		"https://tracker.bt4g.com:443/announce",
		"https://tracker.leechshield.link:443/announce",
		"http://tracker.opentrackr.org:1337/announce",
		"http://tracker.dler.org:6969/announce",
		"http://tracker.corpscorp.online:80/announce",
		"udp://tracker.opentrackr.org:1337/announce",
		"udp://open.demonii.com:1337/announce",
		"udp://open.stealth.si:80/announce",
		"udp://tracker.torrent.eu.org:451/announce",
		"udp://tracker.dler.org:6969/announce"
	};

	static void Apply(libtorrent::add_torrent_params& parameters);
};
}
