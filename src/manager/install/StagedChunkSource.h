#pragma once

#include "domain/interfaces/content/IChunkSource.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

namespace wgrd::manager {

class StagedChunkSource final : public domain::IChunkSource {
public:
    explicit StagedChunkSource(std::filesystem::path chunkFolder);

    ~StagedChunkSource() override;

    [[nodiscard]] std::expected<std::vector<std::byte>, domain::ChunkFetchError> Fetch(
        const domain::ChunkDigest& digest,
        std::uint32_t length) override;

private:
    std::filesystem::path _chunkFolder;
};

}
