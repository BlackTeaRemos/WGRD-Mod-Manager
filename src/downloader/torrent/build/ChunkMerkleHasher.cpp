#include "downloader/torrent/build/ChunkMerkleHasher.h"

#include <libtorrent/hasher.hpp>

#include <algorithm>
#include <utility>

namespace wgrd::downloader {

namespace {

libtorrent::sha256_hash ZeroHash() {
    libtorrent::sha256_hash zero;
    zero.clear();
    return zero;
}

libtorrent::sha256_hash CombinePair(
    const libtorrent::sha256_hash& left,
    const libtorrent::sha256_hash& right) {

    libtorrent::hasher256 hasher;
    hasher.update(left);
    hasher.update(right);
    return hasher.final();
}

}

std::size_t ChunkMerkleHasher::BlockCount(std::uint64_t bytes) {
    if (bytes == 0) {
        return 0;
    }

    return static_cast<std::size_t>((bytes + BLOCK_BYTES - 1) / BLOCK_BYTES);
}

std::size_t ChunkMerkleHasher::NextPowerOfTwo(std::size_t value) {
    std::size_t result = 1;
    while (result < value) {
        result *= 2;
    }
    return result;
}

std::size_t ChunkMerkleHasher::LeafTarget(std::uint64_t fileBytes, std::int32_t pieceBytes) {
    const std::size_t blocksPerPiece = static_cast<std::size_t>(pieceBytes) / BLOCK_BYTES;
    const std::size_t fileBlocks = BlockCount(fileBytes);

    if (fileBlocks <= blocksPerPiece) {
        return NextPowerOfTwo(std::max<std::size_t>(fileBlocks, 1));
    }

    return blocksPerPiece;
}

std::vector<libtorrent::sha256_hash> ChunkMerkleHasher::LeafHashes(std::span<const std::byte> data) {
    std::vector<libtorrent::sha256_hash> leaves;
    leaves.reserve(BlockCount(data.size()));

    std::size_t offset = 0;
    while (offset < data.size()) {
        const std::size_t length = std::min(BLOCK_BYTES, data.size() - offset);
        const std::span<const std::byte> block = data.subspan(offset, length);

        libtorrent::hasher256 hasher;
        hasher.update(libtorrent::span<const char>(
            reinterpret_cast<const char*>(block.data()),
            static_cast<std::ptrdiff_t>(block.size())));

        leaves.push_back(hasher.final());
        offset += length;
    }

    return leaves;
}

libtorrent::sha256_hash ChunkMerkleHasher::Fold(
    std::vector<libtorrent::sha256_hash> leaves,
    std::size_t leafTarget) {

    const libtorrent::sha256_hash zero = ZeroHash();

    if (leafTarget == 0) {
        return zero;
    }

    leaves.resize(leafTarget, zero);

    while (leaves.size() > 1) {
        std::vector<libtorrent::sha256_hash> parents;
        parents.reserve(leaves.size() / 2);

        for (std::size_t index = 0; index + 1 < leaves.size(); index += 2) {
            parents.push_back(CombinePair(leaves[index], leaves[index + 1]));
        }

        leaves = std::move(parents);
    }

    return leaves.front();
}

libtorrent::sha256_hash ChunkMerkleHasher::PieceRoot(
    std::span<const std::byte> pieceData,
    std::size_t leafTarget) {

    return Fold(LeafHashes(pieceData), leafTarget);
}

}
