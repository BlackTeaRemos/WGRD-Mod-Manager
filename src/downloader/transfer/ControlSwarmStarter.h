#pragma once

#include "downloader/storage/ChunkLocator.h"

#include <libtorrent/add_torrent_params.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace wgrd::downloader {
struct PreparedControlTorrent {
	libtorrent::add_torrent_params parameters;
	bool hasV2;
	std::string infoHash;
};

class ControlSwarmStarter {
public:
	[[nodiscard]] static std::optional<PreparedControlTorrent> Prepare(
		ChunkLocator& locator,
		const std::filesystem::path& dataDirectory
	);
};
}
