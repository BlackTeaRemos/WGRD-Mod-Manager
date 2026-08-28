#pragma once

#include "domain/types/content/ChunkSpan.h"

#include <cstddef>
#include <span>
#include <vector>

namespace wgrd::domain {

class IContentChunker {
public:
    virtual ~IContentChunker() = 0;

    [[nodiscard]] virtual std::vector<ChunkSpan> Split(std::span<const std::byte> data) const = 0;
};

inline IContentChunker::~IContentChunker() = default;

}
