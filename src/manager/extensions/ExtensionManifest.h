#pragma once

#include "domain/interfaces/content/extensions/IExtensionPackageVerifier.h"

#include <cstdint>
#include <string_view>

namespace wgrd::manager {
class ExtensionManifest {
public:
    std::string_view version;
    std::string_view imageHash;
    std::uint64_t imageBytes = 0;

    [[nodiscard]] static std::expected<ExtensionManifest, domain::ExtensionPackageError> Parse(
        std::span<const std::byte> bytes
    );

private:
    [[nodiscard]] static bool IsHash_(std::string_view text);
    [[nodiscard]] static bool IsVersion_(std::string_view text);
};
}
