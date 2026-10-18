#pragma once

#include <cstddef>
#include <expected>
#include <span>

namespace wgrd::domain {
enum class ExtensionPackageError {
    InvalidPackage,
    InvalidSignature,
    UnsupportedContract,
    ImageMismatch,
    CryptographyUnavailable
};

class IExtensionPackageVerifier {
public:
    virtual ~IExtensionPackageVerifier() = 0;

    [[nodiscard]] virtual std::expected<void, ExtensionPackageError> Verify(
        std::span<const std::byte> package
    ) const = 0;
};

inline IExtensionPackageVerifier::~IExtensionPackageVerifier() = default;
}
