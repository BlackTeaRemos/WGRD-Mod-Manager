#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace wgrd::tests {
class ApprovedManifestFixture {
public:
    static constexpr std::string_view MANIFEST =
        "WRG-EXTENSION-MANIFEST/1\n"
        "id=blacktearemos.extensions\n"
        "file=blacktearemos_extensions.dll\n"
        "version=1.3.4\n"
        "arch=x86\n"
        "host_api=1.0\n"
        "contract=2\n"
        "game_sha256=5EAA6D6168C9570FF9789BFFB3B2CC16FD908BE61263B12FCC1E8B2F09EFC75D\n"
        "dll_sha256=BAF567F00940AC3D4A6B5799C3C87D70362EC372D58A757AFCA5D4FA01B6CE02\n"
        "dll_size=1608192\n"
        "key_id=6470EC9E9F7A8B7260C7399B37CCA62FA53D2A211D82F7BF3BB2F647893EC170\n";
    static constexpr std::array<std::uint8_t, 64> SIGNATURE{
        0xde, 0x5c, 0xf5, 0xb8, 0xe9, 0x36, 0x7c, 0x49, 0x04, 0x69, 0xdf, 0x46, 0xb8, 0x07, 0x47, 0x1b,
        0xe5, 0x0e, 0x1b, 0x14, 0xb5, 0xb5, 0xd8, 0x92, 0xdd, 0x38, 0xe1, 0xe6, 0x60, 0x53, 0xe0, 0x6f,
        0x33, 0x43, 0x21, 0xd9, 0xd2, 0x97, 0x0c, 0x6e, 0xc5, 0x5b, 0x9f, 0x7b, 0x73, 0x65, 0xfd, 0xd5,
        0xe9, 0x4b, 0x44, 0x07, 0xbe, 0xef, 0xf5, 0x2c, 0xce, 0xf5, 0x49, 0xb1, 0x64, 0x7b, 0x84, 0x90
    };

    static std::span<const std::byte> ManifestBytes() {
        return std::as_bytes(std::span(MANIFEST.data(), MANIFEST.size()));
    }

    static std::span<const std::byte> SignatureBytes() {
        return std::as_bytes(std::span(SIGNATURE));
    }

    static std::vector<std::byte> CorruptImagePackage() {
        std::vector<std::byte> package(1608192);
        const auto manifest = ManifestBytes();
        const auto signature = SignatureBytes();
        package.insert(package.end(), manifest.begin(), manifest.end());
        package.insert(package.end(), signature.begin(), signature.end());
        const std::size_t footerOffset = package.size();
        package.resize(footerOffset + 32);
        static constexpr std::string_view MAGIC = "WRG-EXTENSION-V2";
        for (std::size_t characterIndex = 0; characterIndex < MAGIC.size(); ++characterIndex) {
            package[footerOffset + characterIndex] = static_cast<std::byte>(MAGIC[characterIndex]);
        }
        const std::uint64_t imageBytes = 1608192;
        for (std::size_t byteIndex = 0; byteIndex < 8; ++byteIndex) {
            package[footerOffset + 16 + byteIndex] = static_cast<std::byte>((imageBytes >> (byteIndex * 8)) & 0xFF);
        }
        for (std::size_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
            package[footerOffset + 24 + byteIndex] = static_cast<std::byte>((MANIFEST.size() >> (byteIndex * 8)) & 0xFF);
        }
        return package;
    }

private:
    ApprovedManifestFixture() = delete;
};
}
