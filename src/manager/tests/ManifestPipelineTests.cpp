#include "manager/hash/Blake3Hasher.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/publish/ManifestSigner.h"
#include "manager/publish/PublisherKeyExporter.h"
#include "manager/publish/SigningKeyStore.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"

#include "domain/interfaces/content/IContentChunker.h"
#include "domain/types/content/ChunkSizes.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::ChunkSizes;
using wgrd::domain::ChunkSpan;
using wgrd::domain::DataProtectionError;
using wgrd::domain::IContentChunker;
using wgrd::domain::IDataProtection;
using wgrd::domain::ManifestBuildError;
using wgrd::domain::ManifestDecodeError;
using wgrd::domain::ModManifest;
using wgrd::manager::Blake3Hasher;
using wgrd::manager::DirectoryKeyRegistry;
using wgrd::manager::ManifestAuthenticator;
using wgrd::manager::ManifestBuilder;
using wgrd::manager::ManifestCodec;
using wgrd::manager::ManifestSigner;
using wgrd::manager::PayloadPathPolicy;
using wgrd::manager::PublisherKeyExporter;
using wgrd::manager::SigningKeyStore;

namespace {

class FixedSizeChunker final : public IContentChunker {
public:
    explicit FixedSizeChunker(std::size_t chunkLength)
        : _chunkLength(chunkLength) {
    }

    ~FixedSizeChunker() override = default;

    [[nodiscard]] std::vector<ChunkSpan> Split(std::span<const std::byte> data) const override {
        std::vector<ChunkSpan> spans;

        std::size_t offset = 0;
        while (offset < data.size()) {
            const std::size_t length = std::min(_chunkLength, data.size() - offset);
            spans.push_back(ChunkSpan{offset, length});
            offset += length;
        }

        return spans;
    }

private:
    std::size_t _chunkLength;
};

class TransparentProtection final : public IDataProtection {
public:
    ~TransparentProtection() override = default;

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, DataProtectionError> Protect(
        std::span<const std::uint8_t> plaintext) const override {
        return std::vector<std::uint8_t>(plaintext.begin(), plaintext.end());
    }

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, DataProtectionError> Unprotect(
        std::span<const std::uint8_t> ciphertext) const override {
        return std::vector<std::uint8_t>(ciphertext.begin(), ciphertext.end());
    }
};

class TemporaryTree {
public:
    explicit TemporaryTree(std::string_view label) {
        _root = std::filesystem::temp_directory_path() / "wgrd-pipeline" / label;

        std::error_code failure;
        std::filesystem::remove_all(_root, failure);
        std::filesystem::create_directories(_root, failure);
    }

    ~TemporaryTree() {
        std::error_code failure;
        std::filesystem::remove_all(_root, failure);
    }

    [[nodiscard]] const std::filesystem::path& Root() const {
        return _root;
    }

    void WriteFile(std::string_view relativePath, std::size_t length, std::uint8_t seed) const {
        const std::filesystem::path target = _root / relativePath;

        std::error_code failure;
        std::filesystem::create_directories(target.parent_path(), failure);

        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        for (std::size_t position = 0; position < length; ++position) {
            output.put(static_cast<char>((position + seed) & 0xFF));
        }
    }

private:
    std::filesystem::path _root;
};

}

TEST_CASE("builder describes a real pack layout") {
    const TemporaryTree tree("build");
    tree.WriteFile("mod.json", 64, 1);
    tree.WriteFile("131544/NDF_Win.dat", 5000, 2);
    tree.WriteFile("48574/ZZ_1.dat", 3000, 3);

    const FixedSizeChunker chunker(1024);
    const Blake3Hasher hasher;
    const PayloadPathPolicy policy;
    const ManifestBuilder builder(chunker, hasher, policy);

    const auto fingerprint = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
    REQUIRE(fingerprint.has_value());

    const auto manifest = builder.Build(tree.Root(), *fingerprint, "angel_maps", 3);

    REQUIRE(manifest.has_value());
    REQUIRE(manifest->Files().size() == 3);
    REQUIRE(manifest->TotalBytes() == 64 + 5000 + 3000);
    REQUIRE(manifest->Identifier() == "0011223344556677/angel_maps");

    REQUIRE(manifest->Files()[0].path == "131544/NDF_Win.dat");
    REQUIRE(manifest->Files()[1].path == "48574/ZZ_1.dat");
    REQUIRE(manifest->Files()[2].path == "mod.json");

    REQUIRE(manifest->Files()[0].chunks.size() == 5);
    REQUIRE(manifest->Files()[0].chunks.back().length == 5000 - 4096);
}

TEST_CASE("builder rejects a folder holding a foreign extension") {
    const TemporaryTree tree("foreign");
    tree.WriteFile("payload.dat", 128, 1);
    tree.WriteFile("installer.exe", 128, 2);

    const FixedSizeChunker chunker(1024);
    const Blake3Hasher hasher;
    const PayloadPathPolicy policy;
    const ManifestBuilder builder(chunker, hasher, policy);

    const auto fingerprint = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
    REQUIRE(fingerprint.has_value());

    const auto manifest = builder.Build(tree.Root(), *fingerprint, "bad_mod", 1);

    REQUIRE_FALSE(manifest.has_value());
    REQUIRE(manifest.error() == ManifestBuildError::PathRejected);
}

TEST_CASE("builder rejects an unusable mod name") {
    const TemporaryTree tree("badname");
    tree.WriteFile("payload.dat", 128, 1);

    const FixedSizeChunker chunker(1024);
    const Blake3Hasher hasher;
    const PayloadPathPolicy policy;
    const ManifestBuilder builder(chunker, hasher, policy);

    const auto fingerprint = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
    REQUIRE(fingerprint.has_value());

    REQUIRE_FALSE(builder.Build(tree.Root(), *fingerprint, "Bad Name", 1).has_value());
    REQUIRE_FALSE(builder.Build(tree.Root(), *fingerprint, "", 1).has_value());
}

TEST_CASE("builder is deterministic across runs") {
    const TemporaryTree tree("deterministic");
    tree.WriteFile("a.dat", 2500, 7);
    tree.WriteFile("nested/b.dat", 900, 9);

    const FixedSizeChunker chunker(1024);
    const Blake3Hasher hasher;
    const PayloadPathPolicy policy;
    const ManifestBuilder builder(chunker, hasher, policy);
    const ManifestCodec codec(policy);

    const auto fingerprint = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
    REQUIRE(fingerprint.has_value());

    const auto first = builder.Build(tree.Root(), *fingerprint, "mod", 1);
    const auto second = builder.Build(tree.Root(), *fingerprint, "mod", 1);

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(codec.Encode(*first) == codec.Encode(*second));
}

TEST_CASE("codec round trip preserves the manifest") {
    const TemporaryTree tree("codec");
    tree.WriteFile("mod.json", 32, 1);
    tree.WriteFile("packs/ZZ_Win.dat", 4096, 5);

    const FixedSizeChunker chunker(1000);
    const Blake3Hasher hasher;
    const PayloadPathPolicy policy;
    const ManifestBuilder builder(chunker, hasher, policy);
    const ManifestCodec codec(policy);

    const auto fingerprint = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
    REQUIRE(fingerprint.has_value());

    const auto manifest = builder.Build(tree.Root(), *fingerprint, "round_trip", 42);
    REQUIRE(manifest.has_value());

    const std::vector<std::uint8_t> encoded = codec.Encode(*manifest);
    const auto decoded = codec.Decode(encoded);

    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == *manifest);
}

TEST_CASE("codec rejects a traversal path inside the payload") {
    const PayloadPathPolicy policy;
    const ManifestCodec codec(policy);

    const std::string text =
        R"({"publisher":"0011223344556677","mod":"evil","version":1,)"
        R"("files":[{"path":"../escape.dat","size":4,)"
        R"("chunks":[{"digest":")" + std::string(64, 'a') + R"(","length":4}]}]})";

    const std::vector<std::uint8_t> payload(text.begin(), text.end());
    const auto decoded = codec.Decode(payload);

    REQUIRE_FALSE(decoded.has_value());
    REQUIRE(decoded.error() == ManifestDecodeError::PathRejected);
}

TEST_CASE("codec rejects chunk lengths that disagree with file size") {
    const PayloadPathPolicy policy;
    const ManifestCodec codec(policy);

    const std::string text =
        R"({"publisher":"0011223344556677","mod":"liar","version":1,)"
        R"("files":[{"path":"payload.dat","size":9999,)"
        R"("chunks":[{"digest":")" + std::string(64, 'a') + R"(","length":4}]}]})";

    const std::vector<std::uint8_t> payload(text.begin(), text.end());
    const auto decoded = codec.Decode(payload);

    REQUIRE_FALSE(decoded.has_value());
    REQUIRE(decoded.error() == ManifestDecodeError::ChunkLayoutInvalid);
}

TEST_CASE("codec rejects malformed bytes") {
    const PayloadPathPolicy policy;
    const ManifestCodec codec(policy);

    const std::string text = "not json at all";
    const std::vector<std::uint8_t> payload(text.begin(), text.end());

    const auto decoded = codec.Decode(payload);

    REQUIRE_FALSE(decoded.has_value());
    REQUIRE(decoded.error() == ManifestDecodeError::Malformed);
}

TEST_CASE("publish and consume round trip without an index") {
    const TemporaryTree tree("endtoend");
    tree.WriteFile("mod.json", 48, 1);
    tree.WriteFile("131544/NDF_Win.dat", 9000, 4);

    const TemporaryTree dataDirectory("endtoend-data");
    const TransparentProtection protection;

    SigningKeyStore keyStore(dataDirectory.Root() / "publisher.key", protection);
    const auto identity = keyStore.Create();
    REQUIRE(identity.has_value());

    const PublisherKeyExporter exporter(dataDirectory.Root() / "registry");
    const auto exported = exporter.Export(*identity, "tester", "2026-08-26");
    REQUIRE(exported.has_value());

    const FixedSizeChunker chunker(2048);
    const Blake3Hasher hasher;
    const PayloadPathPolicy policy;
    const ManifestBuilder builder(chunker, hasher, policy);
    const ManifestCodec codec(policy);

    const auto manifest = builder.Build(tree.Root(), identity->fingerprint, "angel_maps", 7);
    REQUIRE(manifest.has_value());

    const ManifestSigner signer(keyStore);
    const auto sealed = signer.Seal(codec.Encode(*manifest));
    REQUIRE(sealed.has_value());

    DirectoryKeyRegistry registry(dataDirectory.Root() / "registry");
    REQUIRE(registry.Count() == 1);
    REQUIRE(registry.IsUsable(identity->fingerprint));

    const ManifestAuthenticator authenticator(registry);
    const auto authenticated = authenticator.Authenticate(*sealed);
    REQUIRE(authenticated.has_value());

    const auto received = codec.Decode(authenticated->payload);
    REQUIRE(received.has_value());
    REQUIRE(*received == *manifest);
    REQUIRE(received->Publisher() == identity->fingerprint);
}

TEST_CASE("directory registry honours a revocation file") {
    const TemporaryTree dataDirectory("revocation");
    const TransparentProtection protection;

    SigningKeyStore keyStore(dataDirectory.Root() / "publisher.key", protection);
    const auto identity = keyStore.Create();
    REQUIRE(identity.has_value());

    const std::filesystem::path registryFolder = dataDirectory.Root() / "registry";
    const PublisherKeyExporter exporter(registryFolder);
    REQUIRE(exporter.Export(*identity, "tester", "2026-08-26").has_value());

    DirectoryKeyRegistry registry(registryFolder);
    REQUIRE(registry.IsUsable(identity->fingerprint));

    std::error_code failure;
    std::filesystem::create_directories(registryFolder / "revoked", failure);

    const std::string fingerprint = identity->fingerprint.ToHex();
    std::ofstream revocation(registryFolder / "revoked" / (fingerprint + ".json"), std::ios::binary);
    revocation << R"({"fingerprint":")" << fingerprint
               << R"(","revokedAt":"2026-08-26","reason":"test"})";
    revocation.close();

    registry.Reload();

    REQUIRE_FALSE(registry.IsUsable(identity->fingerprint));
    REQUIRE(registry.Find(identity->fingerprint).has_value());
}

TEST_CASE("directory registry ignores a key whose fingerprint lies") {
    const TemporaryTree dataDirectory("liar-key");
    const std::filesystem::path keysFolder = dataDirectory.Root() / "registry" / "keys";

    std::error_code failure;
    std::filesystem::create_directories(keysFolder, failure);

    const std::string fingerprint = "0011223344556677";
    std::ofstream key(keysFolder / (fingerprint + ".json"), std::ios::binary);
    key << R"({"fingerprint":")" << fingerprint
        << R"(","publicKey":")" << std::string(64, 'b')
        << R"(","publisher":"liar","addedAt":"2026-08-26"})";
    key.close();

    const DirectoryKeyRegistry registry(dataDirectory.Root() / "registry");

    REQUIRE(registry.Count() == 0);
}
