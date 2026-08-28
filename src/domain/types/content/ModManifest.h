#pragma once

#include "domain/types/content/ManifestFile.h"
#include "domain/types/identity/PublisherFingerprint.h"

#include <cstdint>
#include <string>
#include <vector>

namespace wgrd::domain {

class ModManifest {
public:
    ModManifest();

    ModManifest(
        PublisherFingerprint publisher,
        std::string modName,
        std::uint64_t version,
        std::vector<ManifestFile> files);

    [[nodiscard]] const PublisherFingerprint& Publisher() const noexcept;

    [[nodiscard]] const std::string& ModName() const noexcept;

    [[nodiscard]] std::uint64_t Version() const noexcept;

    [[nodiscard]] const std::vector<ManifestFile>& Files() const noexcept;

    [[nodiscard]] std::string Identifier() const;

    [[nodiscard]] std::string TorrentName() const;

    [[nodiscard]] std::uint64_t TotalBytes() const noexcept;

    [[nodiscard]] std::size_t ChunkCount() const noexcept;

    bool operator==(const ModManifest& other) const = default;

private:
    PublisherFingerprint _publisher;
    std::string _modName;
    std::uint64_t _version;
    std::vector<ManifestFile> _files;
};

}
