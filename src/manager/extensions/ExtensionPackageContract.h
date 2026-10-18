#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace wgrd::manager {
class ExtensionPackageContract {
public:
    static constexpr std::string_view FILE_NAME = "blacktearemos_extensions.dll";
    static constexpr std::string_view MAGIC = "WRG-EXTENSION-V2";
    static constexpr std::size_t FOOTER_BYTES = 32;
    static constexpr std::size_t SIGNATURE_BYTES = 64;
    static constexpr std::size_t MIN_PACKAGE_BYTES = FOOTER_BYTES + SIGNATURE_BYTES + 2;
    static constexpr std::size_t MAX_IMAGE_BYTES = 128 * 1024 * 1024;
    static constexpr std::size_t MAX_MANIFEST_BYTES = 4096;
    static constexpr std::size_t MAX_PACKAGE_BYTES = MAX_IMAGE_BYTES + MAX_MANIFEST_BYTES
        + SIGNATURE_BYTES + FOOTER_BYTES;
    static constexpr std::string_view KEY_ID =
        "6470EC9E9F7A8B7260C7399B37CCA62FA53D2A211D82F7BF3BB2F647893EC170";
    static constexpr std::array<std::uint8_t, 72> PUBLIC_KEY{
        0x45, 0x43, 0x53, 0x31, 0x20, 0x00, 0x00, 0x00, 0x6B, 0x3C, 0x83, 0x01,
        0x14, 0xFB, 0xF3, 0xBB, 0xDA, 0x52, 0xB4, 0x32, 0x2F, 0x06, 0xAA, 0xB4,
        0xB5, 0x79, 0x24, 0x5A, 0x71, 0x7A, 0xD6, 0xA9, 0x53, 0xFA, 0x66, 0xDA,
        0x79, 0xD7, 0x46, 0x6B, 0xC2, 0x7F, 0x1E, 0xBE, 0xC3, 0x19, 0x82, 0xE7,
        0xAA, 0xC9, 0x1B, 0xD2, 0x87, 0xD9, 0x55, 0x04, 0x3B, 0x66, 0xC2, 0xEA,
        0x6B, 0xBD, 0xB2, 0x8D, 0xE7, 0x93, 0x7B, 0x96, 0xE2, 0x07, 0x4E, 0x38
    };

private:
    ExtensionPackageContract() = delete;
};
}
