#pragma once

#include <libtorrent/sha1_hash.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace wgrd::downloader {

class ChunkMerkleHasher {
public:
    static constexpr std::size_t BLOCK_BYTES = 16384;

    [[nodiscard]] static std::size_t BlockCount(std::uint64_t bytes);

    [[nodiscard]] static std::size_t NextPowerOfTwo(std::size_t value);

    [[nodiscard]] static std::size_t LeafTarget(
        std::uint64_t fileBytes,
        std::int32_t pieceBytes);

    [[nodiscard]] static std::vector<libtorrent::sha256_hash> LeafHashes(
        std::span<const std::byte> data);

    [[nodiscard]] static libtorrent::sha256_hash Fold(
        std::vector<libtorrent::sha256_hash> leaves,
        std::size_t leafTarget);

    [[nodiscard]] static libtorrent::sha256_hash PieceRoot(
        std::span<const std::byte> pieceData,
        std::size_t leafTarget);
};

}
