#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace wgrd::domain {

enum class ManifestSignerError {
    PayloadEmpty,
    PayloadTooLarge,
    KeyUnavailable,
    SigningFailed
};

class IManifestSigner {
public:
    virtual ~IManifestSigner() = 0;

    [[nodiscard]] virtual std::expected<std::vector<std::uint8_t>, ManifestSignerError> Seal(
        std::span<const std::uint8_t> payload) const = 0;
};

inline IManifestSigner::~IManifestSigner() = default;

}
