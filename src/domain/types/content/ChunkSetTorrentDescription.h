#pragma once

#include "domain/types/content/ChunkDigest.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wgrd::domain {

struct ChunkSetTorrentDescription {
    std::vector<char> bencoded;
    ChunkDigest infoHash;
    std::uint64_t payloadBytes;
    std::size_t payloadFiles;
};

}
