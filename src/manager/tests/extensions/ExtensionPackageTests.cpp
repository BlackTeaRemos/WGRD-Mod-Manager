#include "manager/extensions/ExtensionCryptography.h"
#include "manager/extensions/ExtensionManifest.h"
#include "manager/extensions/ExtensionPackageContract.h"
#include "manager/extensions/SignedExtensionVerifier.h"
#include "ApprovedManifestFixture.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("Pinned signature accepted") {
    const auto approved = wgrd::manager::ExtensionCryptography::VerifySignature(
        wgrd::tests::ApprovedManifestFixture::ManifestBytes(),
        wgrd::tests::ApprovedManifestFixture::SignatureBytes());
    REQUIRE(approved.has_value());
}

TEST_CASE("Tampered manifest rejected") {
    std::string manifest(wgrd::tests::ApprovedManifestFixture::MANIFEST);
    manifest[0] = 'X';
    const auto approved = wgrd::manager::ExtensionCryptography::VerifySignature(
        std::as_bytes(std::span(manifest.data(), manifest.size())),
        wgrd::tests::ApprovedManifestFixture::SignatureBytes());
    REQUIRE_FALSE(approved.has_value());
    REQUIRE(approved.error() == wgrd::domain::ExtensionPackageError::InvalidSignature);
}

TEST_CASE("Wrong signature rejected") {
    auto signature = wgrd::tests::ApprovedManifestFixture::SIGNATURE;
    signature[0] ^= 1;
    const auto approved = wgrd::manager::ExtensionCryptography::VerifySignature(
        wgrd::tests::ApprovedManifestFixture::ManifestBytes(), std::as_bytes(std::span(signature)));
    REQUIRE_FALSE(approved.has_value());
}

TEST_CASE("Signed manifest parsed") {
    const auto manifest = wgrd::manager::ExtensionManifest::Parse(
        wgrd::tests::ApprovedManifestFixture::ManifestBytes());
    REQUIRE(manifest.has_value());
    REQUIRE(manifest->version == "1.3.4");
    REQUIRE(manifest->imageBytes == 1608192);
}

TEST_CASE("Corrupt image rejected") {
    const wgrd::manager::SignedExtensionVerifier verifier;
    const auto approved = verifier.Verify(wgrd::tests::ApprovedManifestFixture::CorruptImagePackage());
    REQUIRE_FALSE(approved.has_value());
    REQUIRE(approved.error() == wgrd::domain::ExtensionPackageError::ImageMismatch);
}

TEST_CASE("Malformed footer rejected") {
    auto package = wgrd::tests::ApprovedManifestFixture::CorruptImagePackage();
    package.back() = std::byte{1};
    const wgrd::manager::SignedExtensionVerifier verifier;
    const auto approved = verifier.Verify(package);
    REQUIRE_FALSE(approved.has_value());
    REQUIRE(approved.error() == wgrd::domain::ExtensionPackageError::InvalidPackage);
}

TEST_CASE("Unsigned package rejected") {
    const std::array<std::byte, 64> bytes{};
    const wgrd::manager::SignedExtensionVerifier verifier;
    REQUIRE_FALSE(verifier.Verify(bytes).has_value());
}

TEST_CASE("Pinned key identifier matches") {
    const auto digest = wgrd::manager::ExtensionCryptography::Hash(
        std::as_bytes(std::span(wgrd::manager::ExtensionPackageContract::PUBLIC_KEY)));
    REQUIRE(digest.has_value());
    static constexpr char DIGITS[] = "0123456789ABCDEF";
    std::string identifier;
    for (const std::uint8_t value : *digest) {
        identifier.push_back(DIGITS[value >> 4]);
        identifier.push_back(DIGITS[value & 15]);
    }
    REQUIRE(identifier == wgrd::manager::ExtensionPackageContract::KEY_ID);
}
