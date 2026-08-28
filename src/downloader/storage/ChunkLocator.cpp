#include "downloader/storage/ChunkLocator.h"

#include "domain/types/content/ChunkFileNaming.h"

namespace wgrd::downloader {

ChunkLocator::ChunkLocator()
    : _guard(),
      _locations() {
}

void ChunkLocator::RegisterFile(
    std::string fileName,
    const std::filesystem::path& file,
    std::uint64_t offset,
    std::uint64_t length) {

    const std::lock_guard<std::mutex> lock(_guard);

    _locations.insert_or_assign(
        std::move(fileName),
        ChunkLocation{file, offset, static_cast<std::uint32_t>(length)});
}

void ChunkLocator::Register(
    const domain::ModManifest& manifest,
    const std::filesystem::path& modFolder) {

    const std::lock_guard<std::mutex> lock(_guard);

    for (const domain::ManifestFile& file : manifest.Files()) {
        for (const domain::ManifestChunk& chunk : file.chunks) {
            _locations.insert_or_assign(
                domain::ChunkFileNaming::FileNameFor(chunk.digest),
                ChunkLocation{modFolder / file.path, chunk.offset, chunk.length});
        }
    }
}

void ChunkLocator::Forget(const domain::ModManifest& manifest) {
    const std::lock_guard<std::mutex> lock(_guard);

    for (const domain::ManifestFile& file : manifest.Files()) {
        for (const domain::ManifestChunk& chunk : file.chunks) {
            _locations.erase(domain::ChunkFileNaming::FileNameFor(chunk.digest));
        }
    }
}

std::optional<ChunkLocation> ChunkLocator::Find(std::string_view chunkFileName) const {
    const std::string leaf(domain::ChunkFileNaming::LeafOf(chunkFileName));

    const std::lock_guard<std::mutex> lock(_guard);

    const auto match = _locations.find(leaf);
    if (match == _locations.end()) {
        return std::nullopt;
    }

    return match->second;
}

std::size_t ChunkLocator::Count() const {
    const std::lock_guard<std::mutex> lock(_guard);
    return _locations.size();
}

}
