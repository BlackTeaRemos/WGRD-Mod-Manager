#include "downloader/announce/AnnounceGossipPlugin.h"

#include "downloader/announce/AnnounceGossipTorrentPlugin.h"

namespace wgrd::downloader {
AnnounceGossipPlugin::AnnounceGossipPlugin(AnnounceExchange& exchange)
	: _exchange(&exchange) {}

AnnounceGossipPlugin::~AnnounceGossipPlugin() = default;

libtorrent::feature_flags_t AnnounceGossipPlugin::implemented_features() {
	return {};
}

std::shared_ptr<libtorrent::torrent_plugin> AnnounceGossipPlugin::new_torrent(
	const libtorrent::torrent_handle& handle,
	libtorrent::client_data_t
) {
	if (!_exchange->IsControl(handle.info_hashes())) {
		return nullptr;
	}

	return std::make_shared<AnnounceGossipTorrentPlugin>(*_exchange);
}
}
