#include "manager/extensions/ExtensionManifest.h"
#include "manager/extensions/ExtensionPackageContract.h"

#include <array>
#include <charconv>

namespace wgrd::manager {
bool ExtensionManifest::IsHash_(const std::string_view text) {
    return text.size() == 64 && text.find_first_not_of("0123456789ABCDEF") == std::string_view::npos;
}

bool ExtensionManifest::IsVersion_(std::string_view text) {
    for (std::size_t componentIndex = 0; componentIndex < 3; ++componentIndex) {
        const std::size_t separator = text.find('.');
        const std::string_view component = text.substr(0, separator);
        unsigned value = 0;
        const auto parsed = std::from_chars(component.data(), component.data() + component.size(), value);
        if (component.empty() || parsed.ec != std::errc{} || parsed.ptr != component.data() + component.size()
            || value > 65535 || (componentIndex < 2) != (separator != std::string_view::npos)) {
            return false;
        }
        if (separator != std::string_view::npos) {
            text.remove_prefix(separator + 1);
        }
    }
    return true;
}

std::expected<ExtensionManifest, domain::ExtensionPackageError> ExtensionManifest::Parse(
    const std::span<const std::byte> bytes
) {
    std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::array<std::string_view, 11> lines{};
    for (std::string_view& line : lines) {
        const std::size_t separator = text.find('\n');
        if (separator == std::string_view::npos) {
            return std::unexpected(domain::ExtensionPackageError::UnsupportedContract);
        }
        line = text.substr(0, separator);
        text.remove_prefix(separator + 1);
    }
    if (!text.empty() || lines[0] != "WRG-EXTENSION-MANIFEST/1"
        || lines[1] != "id=blacktearemos.extensions" || lines[2] != "file=blacktearemos_extensions.dll"
        || lines[4] != "arch=x86" || lines[5] != "host_api=1.0" || lines[6] != "contract=2"
        || !lines[3].starts_with("version=") || !lines[7].starts_with("game_sha256=")
        || !lines[8].starts_with("dll_sha256=") || !lines[9].starts_with("dll_size=")
        || !lines[10].starts_with("key_id=") || lines[10].substr(7) != ExtensionPackageContract::KEY_ID) {
        return std::unexpected(domain::ExtensionPackageError::UnsupportedContract);
    }
    ExtensionManifest manifest;
    manifest.version = lines[3].substr(8);
    manifest.imageHash = lines[8].substr(11);
    if (!IsVersion_(manifest.version) || !IsHash_(manifest.imageHash) || !IsHash_(lines[7].substr(12))) {
        return std::unexpected(domain::ExtensionPackageError::UnsupportedContract);
    }
    const std::string_view size = lines[9].substr(9);
    const auto parsed = std::from_chars(size.data(), size.data() + size.size(), manifest.imageBytes);
    if (parsed.ec != std::errc{} || parsed.ptr != size.data() + size.size()
        || manifest.imageBytes == 0 || manifest.imageBytes > ExtensionPackageContract::MAX_IMAGE_BYTES) {
        return std::unexpected(domain::ExtensionPackageError::InvalidPackage);
    }
    return manifest;
}
}
