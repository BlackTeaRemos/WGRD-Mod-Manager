#pragma once

#include "domain/interfaces/content/extensions/IExtensionPackageVerifier.h"
#include <cstdint>

namespace wgrd::manager {
class SignedExtensionVerifier final : public domain::IExtensionPackageVerifier {
public:
    SignedExtensionVerifier() = default;
    ~SignedExtensionVerifier() override = default;

    [[nodiscard]] std::expected<void, domain::ExtensionPackageError> Verify(
        std::span<const std::byte> package
    ) const override;

private:
    [[nodiscard]] static std::uint64_t ReadUnsigned_(std::span<const std::byte> bytes);
};
}
