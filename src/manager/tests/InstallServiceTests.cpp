#include "manager/hash/Blake3Hasher.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/announce/AnnounceCodec.h"
#include "manager/service/InstallService.h"
#include "manager/service/PublishService.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"

#include "domain/interfaces/content/IChunkFetcher.h"
#include "domain/interfaces/content/IChunkSetTorrentBuilder.h"
#include "domain/interfaces/content/IContentChunker.h"
#include "domain/types/content/ChunkFileNaming.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <span>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkFileNaming;
using wgrd::domain::ChunkSpan;
using wgrd::domain::DataProtectionError;
using wgrd::domain::IChunkFetcher;
using wgrd::domain::IContentChunker;
using wgrd::domain::IDataProtection;
using wgrd::domain::InstallPhase;
using wgrd::domain::ModManifest;
using wgrd::manager::Blake3Hasher;
using wgrd::manager::InstallService;
using wgrd::manager::ManifestBuilder;
using wgrd::manager::ManifestCodec;
using wgrd::manager::PayloadPathPolicy;
using wgrd::manager::PublishService;

namespace {

constexpr std::size_t CHUNK_LENGTH = 2048;

class FixedSizeChunker final : public IContentChunker {
public:
    ~FixedSizeChunker() override = default;

    [[nodiscard]] std::vector<ChunkSpan> Split(std::span<const std::byte> data) const override {
        std::vector<ChunkSpan> spans;

        std::size_t offset = 0;
        while (offset < data.size()) {
            const std::size_t length = std::min(CHUNK_LENGTH, data.size() - offset);
            spans.push_back(ChunkSpan{offset, length});
            offset += length;
        }

        return spans;
    }
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

class StubTorrentBuilder final : public wgrd::domain::IChunkSetTorrentBuilder {
public:
    ~StubTorrentBuilder() override = default;

    [[nodiscard]] std::expected<wgrd::domain::ChunkSetTorrentDescription, wgrd::domain::ChunkSetTorrentError>
    Build(
        const ModManifest& manifest,
        const std::filesystem::path&,
        std::span<const std::uint8_t>) const override {
        const auto infoHash = ChunkDigest::FromHex(std::string(64, 'a'));
        REQUIRE(infoHash.has_value());

        return wgrd::domain::ChunkSetTorrentDescription{
            {'d', 'e'},
            *infoHash,
            manifest.TotalBytes(),
            manifest.ChunkCount()
        };
    }
};

class CopyingFetcher final : public IChunkFetcher {
public:
    CopyingFetcher(
        const ModManifest& manifest,
        std::filesystem::path sourceFolder,
        std::filesystem::path sealedManifestPath = {})
        : _manifest(manifest),
          _sourceFolder(std::move(sourceFolder)),
          _sealedManifestPath(std::move(sealedManifestPath)),
          _status() {
    }

    ~CopyingFetcher() override = default;

    [[nodiscard]] std::expected<void, wgrd::domain::FetchError> Begin(
        std::string identifier,
        const ChunkDigest&,
        const std::filesystem::path& stagingFolder,
        const std::vector<std::string>& wantedFiles) override {

        const std::filesystem::path target = stagingFolder / _manifest.TorrentName();

        std::error_code failure;
        std::filesystem::create_directories(target, failure);

        for (const std::string& wantedName : wantedFiles) {
            if (wantedName == ChunkFileNaming::MANIFEST_FILE) {
                std::filesystem::copy_file(
                    _sealedManifestPath,
                    target / wantedName,
                    std::filesystem::copy_options::overwrite_existing,
                    failure);
                ++_served;
                continue;
            }

            for (const auto& file : _manifest.Files()) {
                for (const auto& chunk : file.chunks) {
                    if (ChunkFileNaming::FileNameFor(chunk.digest) != wantedName) {
                        continue;
                    }

                    std::ifstream input(_sourceFolder / file.path, std::ios::binary);
                    input.seekg(static_cast<std::streamoff>(chunk.offset));

                    std::vector<char> bytes(chunk.length);
                    input.read(bytes.data(), static_cast<std::streamsize>(chunk.length));

                    std::ofstream output(
                        target / wantedName,
                        std::ios::binary | std::ios::trunc);

                    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                    ++_served;
                }
            }
        }

        _status.phase = wgrd::domain::FetchPhase::Complete;
        _status.identifier = std::move(identifier);
        _status.stagingFolder = stagingFolder;

        return {};
    }

    [[nodiscard]] wgrd::domain::FetchStatus Fetch() const override {
        return _status;
    }

    void Cancel() override {
        _status = wgrd::domain::FetchStatus{};
    }

    [[nodiscard]] std::size_t Served() const {
        return _served;
    }

private:
    ModManifest _manifest;
    std::filesystem::path _sourceFolder;
    std::filesystem::path _sealedManifestPath;
    wgrd::domain::FetchStatus _status;
    std::size_t _served = 0;
};

class TemporaryTree {
public:
    explicit TemporaryTree(std::string_view label) {
        _root = std::filesystem::temp_directory_path() / "wgrd-install" / label;

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

private:
    std::filesystem::path _root;
};

void WritePayload(const std::filesystem::path& target, std::uint64_t bytes, std::uint8_t seed) {
    std::error_code failure;
    std::filesystem::create_directories(target.parent_path(), failure);

    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    for (std::uint64_t position = 0; position < bytes; ++position) {
        output.put(static_cast<char>(((position * 13) ^ (position >> 8) ^ seed) & 0xFF));
    }
}

std::vector<char> ReadAll(const std::filesystem::path& source) {
    std::ifstream input(source, std::ios::binary);
    return std::vector<char>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

bool AwaitPhase(InstallService& service, InstallPhase phase, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        service.Poll();

        const InstallPhase current = service.Progress().phase;
        if (current == phase) {
            return true;
        }

        if (current == InstallPhase::Failed) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

}

TEST_CASE("install fetches a published mod into the mods folder") {
    const TemporaryTree publisher("publisher");
    const TemporaryTree consumer("consumer");
    const TemporaryTree data("data");

    const std::filesystem::path publisherMods = publisher.Root() / "Mods";
    const std::filesystem::path consumerMods = consumer.Root() / "Mods";

    WritePayload(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat", 9000, 5);
    WritePayload(publisherMods / "angel_maps" / "mod.json", 200, 9);

    const FixedSizeChunker chunker;
    const Blake3Hasher hasher;
    const PayloadPathPolicy pathPolicy;
    const TransparentProtection protection;
    const StubTorrentBuilder torrentBuilder;
    const ManifestCodec codec(pathPolicy);
    const ManifestBuilder manifestBuilder(chunker, hasher, pathPolicy);

    wgrd::manager::DirectoryKeyRegistry registry(data.Root() / "registry");
    wgrd::manager::AnnounceReceiver receiver(registry);

    PublishService publishService(
        publisherMods,
        data.Root(),
        chunker,
        hasher,
        pathPolicy,
        protection,
        torrentBuilder,
        receiver);

    REQUIRE(publishService.CreateKey("tester").has_value());

    registry.Reload();

    const auto published = publishService.Publish("angel_maps");
    REQUIRE(published.has_value());

    const auto announce = receiver.Retained(published->identifier);
    REQUIRE(announce.has_value());

    const auto sealed = publishService.Store().Load(announce->manifestDigest.ToHex());
    REQUIRE(sealed.has_value());

    const wgrd::manager::ManifestAuthenticator authenticator(registry);
    const auto authenticated = authenticator.Authenticate(*sealed);
    REQUIRE(authenticated.has_value());

    const auto manifest = codec.Decode(authenticated->payload);
    REQUIRE(manifest.has_value());

    CopyingFetcher fetcher(*manifest, publisherMods / "angel_maps");

    InstallService installService(
        consumerMods,
        data.Root(),
        receiver,
        publishService.Store(),
        authenticator,
        codec,
        manifestBuilder,
        hasher,
        fetcher);

    REQUIRE(installService.Start(published->identifier).has_value());

    REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

    const wgrd::domain::InstallProgress progress = installService.Progress();

    REQUIRE(progress.remoteChunks == manifest->ChunkCount());
    REQUIRE(fetcher.Served() > 0);
    REQUIRE(progress.heldBytes == 0);

    const std::filesystem::path installed = consumerMods / "angel_maps";

    REQUIRE(ReadAll(installed / "packs" / "ZZ_Win.dat")
        == ReadAll(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat"));

    REQUIRE(ReadAll(installed / "mod.json")
        == ReadAll(publisherMods / "angel_maps" / "mod.json"));

    REQUIRE_FALSE(std::filesystem::exists(
        data.Root() / InstallService::STAGING_FOLDER / manifest->TorrentName()));
}

TEST_CASE("reinstalling an unchanged mod fetches nothing") {
    const TemporaryTree publisher("publisher-reuse");
    const TemporaryTree data("data-reuse");

    const std::filesystem::path publisherMods = publisher.Root() / "Mods";

    WritePayload(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat", 9000, 5);

    const FixedSizeChunker chunker;
    const Blake3Hasher hasher;
    const PayloadPathPolicy pathPolicy;
    const TransparentProtection protection;
    const StubTorrentBuilder torrentBuilder;
    const ManifestCodec codec(pathPolicy);
    const ManifestBuilder manifestBuilder(chunker, hasher, pathPolicy);

    wgrd::manager::DirectoryKeyRegistry registry(data.Root() / "registry");
    wgrd::manager::AnnounceReceiver receiver(registry);

    PublishService publishService(
        publisherMods,
        data.Root(),
        chunker,
        hasher,
        pathPolicy,
        protection,
        torrentBuilder,
        receiver);

    REQUIRE(publishService.CreateKey("tester").has_value());
    registry.Reload();

    const auto published = publishService.Publish("angel_maps");
    REQUIRE(published.has_value());

    const auto announce = receiver.Retained(published->identifier);
    REQUIRE(announce.has_value());

    const auto sealed = publishService.Store().Load(announce->manifestDigest.ToHex());
    REQUIRE(sealed.has_value());

    const wgrd::manager::ManifestAuthenticator authenticator(registry);
    const auto authenticated = authenticator.Authenticate(*sealed);
    REQUIRE(authenticated.has_value());

    const auto manifest = codec.Decode(authenticated->payload);
    REQUIRE(manifest.has_value());

    CopyingFetcher fetcher(*manifest, publisherMods / "angel_maps");

    InstallService installService(
        publisherMods,
        data.Root(),
        receiver,
        publishService.Store(),
        authenticator,
        codec,
        manifestBuilder,
        hasher,
        fetcher);

    REQUIRE(installService.Start(published->identifier).has_value());
    REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

    REQUIRE(installService.Progress().remoteChunks == 0);
    REQUIRE(fetcher.Served() == 0);
    REQUIRE(installService.Progress().heldBytes == manifest->TotalBytes());
}

TEST_CASE("install pulls the manifest when only the announce is known") {
    const TemporaryTree publisher("pub-pull");
    const TemporaryTree consumer("con-pull");

    const std::filesystem::path publisherMods = publisher.Root() / "Mods";
    const std::filesystem::path consumerMods = consumer.Root() / "Mods";

    WritePayload(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat", 9000, 5);

    const FixedSizeChunker chunker;
    const Blake3Hasher hasher;
    const PayloadPathPolicy pathPolicy;
    const TransparentProtection protection;
    const StubTorrentBuilder torrentBuilder;
    const ManifestCodec codec(pathPolicy);
    const ManifestBuilder manifestBuilder(chunker, hasher, pathPolicy);

    wgrd::manager::DirectoryKeyRegistry registry(publisher.Root() / "registry");
    wgrd::manager::AnnounceReceiver publisherReceiver(registry);

    PublishService publishService(
        publisherMods,
        publisher.Root(),
        chunker,
        hasher,
        pathPolicy,
        protection,
        torrentBuilder,
        publisherReceiver);

    REQUIRE(publishService.CreateKey("tester").has_value());
    registry.Reload();

    const auto published = publishService.Publish("angel_maps");
    REQUIRE(published.has_value());

    const auto announce = publisherReceiver.Retained(published->identifier);
    REQUIRE(announce.has_value());

    const std::filesystem::path sealedPath =
        publishService.Store().PathFor(announce->manifestDigest.ToHex());
    REQUIRE(std::filesystem::exists(sealedPath));

    const wgrd::manager::ManifestAuthenticator authenticator(registry);

    const auto sealed = publishService.Store().Load(announce->manifestDigest.ToHex());
    REQUIRE(sealed.has_value());
    const auto authenticated = authenticator.Authenticate(*sealed);
    REQUIRE(authenticated.has_value());
    const auto manifest = codec.Decode(authenticated->payload);
    REQUIRE(manifest.has_value());

    wgrd::manager::AnnounceReceiver consumerReceiver(registry);
    REQUIRE(consumerReceiver.Accept(
        wgrd::manager::AnnounceCodec::Encode(*announce)).has_value());

    const wgrd::manager::ManifestStore consumerStore(consumer.Root() / "manifests");
    REQUIRE_FALSE(consumerStore.Holds(announce->manifestDigest.ToHex()));

    CopyingFetcher fetcher(*manifest, publisherMods / "angel_maps", sealedPath);

    InstallService installService(
        consumerMods,
        consumer.Root(),
        consumerReceiver,
        consumerStore,
        authenticator,
        codec,
        manifestBuilder,
        hasher,
        fetcher);

    REQUIRE(installService.Start(published->identifier).has_value());
    REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

    REQUIRE(consumerStore.Holds(announce->manifestDigest.ToHex()));

    REQUIRE(ReadAll(consumerMods / "angel_maps" / "packs" / "ZZ_Win.dat")
        == ReadAll(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat"));
}