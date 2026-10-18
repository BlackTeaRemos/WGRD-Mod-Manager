#include "manager/extensions/SignedExtensionVerifier.h"
#include "manager/extensions/ExtensionCryptography.h"
#include "manager/extensions/ExtensionManifest.h"
#include "manager/extensions/ExtensionPackageContract.h"

#include <string>

namespace wgrd::manager {
std::uint64_t SignedExtensionVerifier::ReadUnsigned_(const std::span<const std::byte> bytes) {
    std::uint64_t value = 0;
    for (std::size_t byteIndex = 0; byteIndex < bytes.size(); ++byteIndex) {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[byteIndex])) << (byteIndex * 8);
    }
    return value;
}

std::expected<void, domain::ExtensionPackageError> SignedExtensionVerifier::Verify(
    const std::span<const std::byte> package
) const {
    if (package.size() < ExtensionPackageContract::MIN_PACKAGE_BYTES
        || package.size() > ExtensionPackageContract::MAX_PACKAGE_BYTES) {
        return std::unexpected(domain::ExtensionPackageError::InvalidPackage);
    }
    const auto footer = package.last(ExtensionPackageContract::FOOTER_BYTES);
    const std::string_view magic(reinterpret_cast<const char*>(footer.data()), 16);
    if (magic != ExtensionPackageContract::MAGIC) {
        return std::unexpected(domain::ExtensionPackageError::InvalidPackage);
    }
    const std::uint64_t imageBytes = ReadUnsigned_(footer.subspan(16, 8));
    const std::uint64_t manifestBytes = ReadUnsigned_(footer.subspan(24, 4));
    if (ReadUnsigned_(footer.subspan(28, 4)) != 0 || imageBytes == 0
        || imageBytes > ExtensionPackageContract::MAX_IMAGE_BYTES || manifestBytes == 0
        || manifestBytes > ExtensionPackageContract::MAX_MANIFEST_BYTES
        || imageBytes + manifestBytes + ExtensionPackageContract::SIGNATURE_BYTES
            + ExtensionPackageContract::FOOTER_BYTES != package.size()) {
        return std::unexpected(domain::ExtensionPackageError::InvalidPackage);
    }
    const auto image = package.first(static_cast<std::size_t>(imageBytes));
    const auto manifest = package.subspan(static_cast<std::size_t>(imageBytes),
        static_cast<std::size_t>(manifestBytes));
    const auto signature = package.subspan(static_cast<std::size_t>(imageBytes + manifestBytes),
        ExtensionPackageContract::SIGNATURE_BYTES);
    const auto approved = ExtensionCryptography::VerifySignature(manifest, signature);
    if (!approved.has_value()) {
        return std::unexpected(approved.error());
    }
    const auto description = ExtensionManifest::Parse(manifest);
    if (!description.has_value()) {
        return std::unexpected(description.error());
    }
    if (description->imageBytes != imageBytes) {
        return std::unexpected(domain::ExtensionPackageError::ImageMismatch);
    }
    const auto digest = ExtensionCryptography::Hash(image);
    if (!digest.has_value()) {
        return std::unexpected(digest.error());
    }
    static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    std::string imageHash;
    for (const std::uint8_t value : *digest) {
        imageHash.push_back(HEX_DIGITS[value >> 4]);
        imageHash.push_back(HEX_DIGITS[value & 15]);
    }
    if (imageHash != description->imageHash) {
        return std::unexpected(domain::ExtensionPackageError::ImageMismatch);
    }
    const std::string_view imageText(reinterpret_cast<const char*>(image.data()), image.size());
    if (imageText.find("BLACKTEAREMOS_DIAGNOSTIC_BUILD") != std::string_view::npos
        || imageText.find("fpv-descriptor-lifetime-v1") != std::string_view::npos
        || imageText.find("fpv_descriptor_lifetime.log") != std::string_view::npos) {
        return std::unexpected(domain::ExtensionPackageError::UnsupportedContract);
    }
    return {};
}
}
