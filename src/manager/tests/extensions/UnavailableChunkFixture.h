#pragma once

#include "domain/interfaces/content/IChunkSource.h"

namespace wgrd::tests {
class UnavailableChunkFixture final : public domain::IChunkSource {
public:
    UnavailableChunkFixture() = default;
    ~UnavailableChunkFixture() override = default;

    std::expected<std::vector<std::byte>, domain::ChunkFetchError> Fetch(
        const domain::ChunkDigest&, std::uint32_t
    ) override {
        return std::unexpected(domain::ChunkFetchError::Unavailable);
    }
};
}
