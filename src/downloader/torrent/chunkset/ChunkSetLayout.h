#pragma once

#include "domain/types/content/ChunkDigest.h"
#include "domain/types/content/ModManifest.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::downloader {

struct ChunkSetEntry {
    std::string fileName;
    domain::ChunkDigest digest;
    std::uint32_t length;

    bool operator==(const ChunkSetEntry& other) const = default;
};

class ChunkSetLayout {
public:
    static constexpr std::string_view CHUNK_SUFFIX = ".chunk";

    [[nodiscard]] static std::vector<ChunkSetEntry> Describe(const domain::ModManifest& manifest);

    [[nodiscard]] static std::uint64_t TotalBytes(std::span<const ChunkSetEntry> entries);

    [[nodiscard]] static std::string FileNameFor(const domain::ChunkDigest& digest);
};

}
