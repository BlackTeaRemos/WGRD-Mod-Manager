#include "downloader/announce/AnnounceGossipTorrentPlugin.h"

#include "downloader/announce/AnnounceGossipPeerPlugin.h"

namespace wgrd::downloader {
AnnounceGossipTorrentPlugin::AnnounceGossipTorrentPlugin(AnnounceExchange& exchange)
	: _exchange(&exchange) {}

AnnounceGossipTorrentPlugin::~AnnounceGossipTorrentPlugin() = default;

std::shared_ptr<libtorrent::peer_plugin> AnnounceGossipTorrentPlugin::new_connection(
	const libtorrent::peer_connection_handle& connection
) {
	return std::make_shared<AnnounceGossipPeerPlugin>(connection, *_exchange);
}
}
