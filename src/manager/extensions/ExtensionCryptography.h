#pragma once

#include "domain/interfaces/content/extensions/IExtensionPackageVerifier.h"

#include <array>
#include <cstdint>

namespace wgrd::manager {
class ExtensionCryptography {
public:
    [[nodiscard]] static std::expected<std::array<std::uint8_t, 32>, domain::ExtensionPackageError> Hash(
        std::span<const std::byte> bytes
    );

    [[nodiscard]] static std::expected<void, domain::ExtensionPackageError> VerifySignature(
        std::span<const std::byte> manifest,
        std::span<const std::byte> signature
    );

private:
    ExtensionCryptography() = delete;
};
}
