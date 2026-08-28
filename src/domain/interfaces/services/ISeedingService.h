#pragma once

#include "domain/types/content/ModManifest.h"
#include "domain/types/status/SeedEntry.h"

#include <expected>
#include <filesystem>
#include <vector>

namespace wgrd::domain {

enum class SeedError {
    Disabled,
    AlreadySeeding,
    TorrentBuildFailed,
    SessionRejected
};

class ISeedingService {
public:
    virtual ~ISeedingService() = 0;

    [[nodiscard]] virtual bool Enabled() const = 0;

    virtual void SetEnabled(bool enabled) = 0;

    [[nodiscard]] virtual std::expected<SeedEntry, SeedError> Announce(
        const ModManifest& manifest,
        const std::filesystem::path& modFolder,
        const std::filesystem::path& sealedManifestPath) = 0;

    [[nodiscard]] virtual const std::vector<SeedEntry>& Entries() const = 0;

    [[nodiscard]] virtual std::uint64_t UploadedBytes() const = 0;
};

inline ISeedingService::~ISeedingService() = default;

}
