#pragma once

#include "domain/types/content/ModManifest.h"

#include <cstddef>
#include <expected>
#include <filesystem>

namespace wgrd::downloader {

enum class MaterialiseError {
    SourceUnreadable,
    TargetUnwritable,
    LengthMismatch
};

class ChunkSetMaterialiser {
public:
    [[nodiscard]] static std::expected<std::size_t, MaterialiseError> Write(
        const domain::ModManifest& manifest,
        const std::filesystem::path& modFolder,
        const std::filesystem::path& chunkFolder);
};

}
