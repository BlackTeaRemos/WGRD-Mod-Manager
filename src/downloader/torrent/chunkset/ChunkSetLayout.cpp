#include "downloader/torrent/chunkset/ChunkSetLayout.h"

#include <algorithm>
#include <set>

namespace wgrd::downloader {

std::string ChunkSetLayout::FileNameFor(const domain::ChunkDigest& digest) {
    return digest.ToHex() + std::string(CHUNK_SUFFIX);
}

std::vector<ChunkSetEntry> ChunkSetLayout::Describe(const domain::ModManifest& manifest) {
    std::set<std::string> seen;
    std::vector<ChunkSetEntry> entries;

    for (const domain::ManifestFile& file : manifest.Files()) {
        for (const domain::ManifestChunk& chunk : file.chunks) {
            const std::string hex = chunk.digest.ToHex();
            if (!seen.insert(hex).second) {
                continue;
            }

            entries.push_back(ChunkSetEntry{
                FileNameFor(chunk.digest),
                chunk.digest,
                chunk.length
            });
        }
    }

    std::sort(entries.begin(), entries.end(), [](const ChunkSetEntry& left, const ChunkSetEntry& right) {
        return left.fileName < right.fileName;
    });

    return entries;
}

std::uint64_t ChunkSetLayout::TotalBytes(std::span<const ChunkSetEntry> entries) {
    std::uint64_t total = 0;
    for (const ChunkSetEntry& entry : entries) {
        total += entry.length;
    }
    return total;
}

}
