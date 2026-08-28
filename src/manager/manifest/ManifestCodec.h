#pragma once

#include "domain/interfaces/content/IManifestCodec.h"
#include "domain/interfaces/content/IPayloadPathPolicy.h"
#include "domain/types/content/ManifestFile.h"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace wgrd::manager {

class ManifestCodec final : public domain::IManifestCodec {
public:
    explicit ManifestCodec(const domain::IPayloadPathPolicy& pathPolicy);

    ~ManifestCodec() override;

    [[nodiscard]] std::vector<std::uint8_t> Encode(const domain::ModManifest& manifest) const override;

    [[nodiscard]] std::expected<domain::ModManifest, domain::ManifestDecodeError> Decode(
        std::span<const std::uint8_t> payload) const override;

private:
    static std::expected<void, domain::ManifestDecodeError> ValidateChunkLayout_(
        const domain::ManifestFile& file);

    const domain::IPayloadPathPolicy* _pathPolicy;
};

}
