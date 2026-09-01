#include "downloader/transfer/TrackerList.h"

#include <algorithm>
#include <string>

namespace wgrd::downloader {
void TrackerList::Apply(libtorrent::add_torrent_params& parameters) {
	for (const std::string_view announce : ANNOUNCE_URLS) {
		const std::string url(announce);

		if (std::ranges::find(parameters.trackers, url) != parameters.trackers.end()) {
			continue;
		}

		parameters.trackers.push_back(url);
		parameters.tracker_tiers.push_back(0);
	}
}
}
