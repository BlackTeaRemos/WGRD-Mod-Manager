#pragma once

#include "downloader/announce/AnnounceExchange.h"

#include <libtorrent/extensions.hpp>

#include <memory>

namespace wgrd::downloader {
class AnnounceGossipPlugin final : public libtorrent::plugin {
public:
	explicit AnnounceGossipPlugin(AnnounceExchange& exchange);

	~AnnounceGossipPlugin() override;

	[[nodiscard]] libtorrent::feature_flags_t implemented_features() override;

	[[nodiscard]] std::shared_ptr<libtorrent::torrent_plugin> new_torrent(
		const libtorrent::torrent_handle& handle,
		libtorrent::client_data_t userData
	) override;

private:
	AnnounceExchange* _exchange;
};
}
