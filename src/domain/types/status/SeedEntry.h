#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace wgrd::domain {

struct SeedEntry {
    std::string identifier;
    std::string modName;
    std::uint64_t version;
    std::uint64_t payloadBytes;
    std::size_t chunkCount;
    std::string infoHash;
    bool seeding;
    std::uint32_t peers;
    std::uint64_t uploadedBytes;

    bool operator==(const SeedEntry& other) const = default;
};

}
