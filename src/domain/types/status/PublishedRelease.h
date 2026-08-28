#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace wgrd::domain {

struct PublishedRelease {
    std::string identifier;
    std::string modName;
    std::uint64_t version;
    std::uint64_t totalBytes;
    std::size_t chunkCount;
    std::size_t fileCount;
    std::string manifestDigest;

    bool operator==(const PublishedRelease& other) const = default;
};

}
