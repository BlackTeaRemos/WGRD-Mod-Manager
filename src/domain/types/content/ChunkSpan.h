#pragma once

#include <cstddef>

namespace wgrd::domain {

struct ChunkSpan {
    std::size_t offset;
    std::size_t length;

    bool operator==(const ChunkSpan& other) const noexcept = default;
};

}
