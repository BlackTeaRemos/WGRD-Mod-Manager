#pragma once

#include "domain/types/content/ModManifest.h"
#include "downloader/torrent/build/ChunkSetTorrent.h"
#include "downloader/torrent/build/TorrentCache.h"

#include <libtorrent/add_torrent_params.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wgrd::downloader {
struct PreparedSeedTorrent {
	libtorrent::add_torrent_params parameters;
	std::string infoHash;
	std::uint64_t payloadBytes;
	std::size_t sealedBytes;
};

class SeedTorrentAssembler {
public:
	[[nodiscard]] static std::optional<PreparedSeedTorrent> Prepare(
		const TorrentCache& torrents,
		const domain::ModManifest& manifest,
		const std::filesystem::path& modFolder,
		const std::filesystem::path& sealedManifestPath,
		const std::filesystem::path& savePath
	);

private:
	[[nodiscard]] static std::vector<std::uint8_t> ReadSealed_(
		const std::filesystem::path& sealedManifestPath
	);

	[[nodiscard]] static std::expected<ChunkSetTorrentBytes, TorrentBuildError> Assemble_(
		const TorrentCache& torrents,
		const domain::ModManifest& manifest,
		const std::filesystem::path& modFolder,
		const std::vector<std::uint8_t>& sealed,
		const std::string& cacheKey
	);

	[[nodiscard]] static bool DescribedMatches_(
		const domain::ModManifest& manifest,
		std::size_t sealedBytes,
		const ChunkSetTorrentBytes& described
	);
};
}
