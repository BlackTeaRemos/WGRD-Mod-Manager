#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace wgrd::downloader {

enum class TorrentBuildError {
    FolderMissing,
    FolderEmpty,
    HashingFailed,
    EncodingFailed
};

struct ChunkSetTorrentBytes {
    std::vector<char> bencoded;
    std::string infoHash;
    std::uint64_t payloadBytes;
    std::size_t payloadFiles;
    std::size_t padFiles;
};

class ChunkSetTorrent {
public:
    static constexpr std::int32_t DEFAULT_PIECE_BYTES = 4 * 1024 * 1024;

    [[nodiscard]] static std::expected<ChunkSetTorrentBytes, TorrentBuildError> Create(
        const std::filesystem::path& chunkFolder,
        std::int32_t pieceBytes = DEFAULT_PIECE_BYTES);
};

}
