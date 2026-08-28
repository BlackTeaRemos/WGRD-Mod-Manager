#pragma once

#include "domain/types/content/ModManifest.h"
#include "downloader/torrent/build/ChunkSetTorrent.h"

#include <cstdint>
#include <cstdint>
#include <expected>
#include <span>
#include <filesystem>
#include <string_view>

namespace wgrd::downloader {

class VirtualChunkSetTorrent {
public:
    [[nodiscard]] static std::expected<ChunkSetTorrentBytes, TorrentBuildError> Create(
        const domain::ModManifest& manifest,
        const std::filesystem::path& modFolder,
        std::string_view torrentName,
        std::span<const std::uint8_t> sealedManifest,
        std::int32_t pieceBytes = ChunkSetTorrent::DEFAULT_PIECE_BYTES);
};

}
