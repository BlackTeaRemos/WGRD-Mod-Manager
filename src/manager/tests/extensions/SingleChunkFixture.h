#pragma once

#include "domain/interfaces/content/IContentChunker.h"

namespace wgrd::tests {
class SingleChunkFixture final : public domain::IContentChunker {
public:
    SingleChunkFixture() = default;
    ~SingleChunkFixture() override = default;

    std::vector<domain::ChunkSpan> Split(const std::span<const std::byte> bytes) const override {
        return {{0, bytes.size()}};
    }
};
}
