#pragma once

#include "downloader/announce/AnnounceExchange.h"

#include <libtorrent/extensions.hpp>
#include <libtorrent/peer_connection_handle.hpp>

#include <memory>

namespace wgrd::downloader {
class AnnounceGossipTorrentPlugin final : public libtorrent::torrent_plugin {
public:
	explicit AnnounceGossipTorrentPlugin(AnnounceExchange& exchange);

	~AnnounceGossipTorrentPlugin() override;

	[[nodiscard]] std::shared_ptr<libtorrent::peer_plugin> new_connection(
		const libtorrent::peer_connection_handle& connection
	) override;

private:
	AnnounceExchange* _exchange;
};
}
