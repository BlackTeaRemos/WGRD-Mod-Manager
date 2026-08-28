#include "downloader/torrent/chunkset/ChunkFolderSource.h"
#include "downloader/torrent/chunkset/ChunkSetLayout.h"
#include "downloader/torrent/chunkset/ChunkSetMaterialiser.h"
#include "downloader/torrent/build/ChunkSetTorrent.h"
#include "downloader/transfer/SwarmNode.h"

#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::ChunkFolderSource;
using wgrd::downloader::ChunkSetLayout;
using wgrd::downloader::ChunkSetMaterialiser;
using wgrd::downloader::ChunkSetTorrent;
using wgrd::downloader::SwarmNode;

namespace {

constexpr std::int32_t TEST_PIECE_BYTES = 16 * 1024;
constexpr std::size_t CHUNK_LENGTH = 4096;
constexpr std::size_t CHUNK_COUNT = 6;

class TemporaryTree {
public:
    explicit TemporaryTree(std::string_view label) {
        _root = std::filesystem::temp_directory_path() / "wgrd-swarm" / label;

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

ChunkDigest MakeDigest(char filler) {
    const auto digest = ChunkDigest::FromHex(std::string(ChunkDigest::HEX_LENGTH, filler));
    REQUIRE(digest.has_value());
    return *digest;
}

std::vector<std::uint8_t> MakePattern(std::size_t length, std::uint8_t seed) {
    std::vector<std::uint8_t> bytes(length);
    for (std::size_t position = 0; position < length; ++position) {
        bytes[position] = static_cast<std::uint8_t>((position * 17 + seed) & 0xFF);
    }
    return bytes;
}

void WriteBytes(const std::filesystem::path& target, const std::vector<std::uint8_t>& bytes) {
    std::error_code failure;
    std::filesystem::create_directories(target.parent_path(), failure);

    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& source) {
    std::ifstream input(source, std::ios::binary);
    const std::istreambuf_iterator<char> first(input);
    const std::istreambuf_iterator<char> last;
    const std::string raw(first, last);

    std::vector<std::uint8_t> bytes;
    bytes.reserve(raw.size());
    for (const char character : raw) {
        bytes.push_back(static_cast<std::uint8_t>(character));
    }
    return bytes;
}

ModManifest MakeManifest() {
    std::vector<ManifestChunk> chunks;
    for (std::size_t index = 0; index < CHUNK_COUNT; ++index) {
        chunks.push_back(ManifestChunk{
            MakeDigest(static_cast<char>('a' + index)),
            static_cast<std::uint64_t>(index * CHUNK_LENGTH),
            static_cast<std::uint32_t>(CHUNK_LENGTH)
        });
    }

    ManifestFile file{"packs/ZZ_Win.dat", CHUNK_COUNT * CHUNK_LENGTH, std::move(chunks)};

    return ModManifest(PublisherFingerprint{}, "angel_maps", 1, {file});
}

}

TEST_CASE("layout names one file per unique chunk") {
    const ModManifest manifest = MakeManifest();
    const std::vector<wgrd::downloader::ChunkSetEntry> entries = ChunkSetLayout::Describe(manifest);

    REQUIRE(entries.size() == CHUNK_COUNT);
    REQUIRE(ChunkSetLayout::TotalBytes(entries) == CHUNK_COUNT * CHUNK_LENGTH);
    REQUIRE(entries.front().fileName == MakeDigest('a').ToHex() + ".chunk");
}

TEST_CASE("layout collapses a repeated chunk to one file") {
    const ChunkDigest repeated = MakeDigest('a');

    std::vector<ManifestChunk> chunks{
        ManifestChunk{repeated, 0, 16},
        ManifestChunk{repeated, 16, 16}
    };

    const ManifestFile file{"payload.dat", 32, std::move(chunks)};
    const ModManifest manifest(PublisherFingerprint{}, "mod", 1, {file});

    REQUIRE(ChunkSetLayout::Describe(manifest).size() == 1);
}

TEST_CASE("materialiser writes chunk files matching the source bytes") {
    const TemporaryTree tree("materialise");
    const ModManifest manifest = MakeManifest();

    const std::vector<std::uint8_t> content = MakePattern(CHUNK_COUNT * CHUNK_LENGTH, 11);
    WriteBytes(tree.Root() / "mod" / "packs" / "ZZ_Win.dat", content);

    const auto written = ChunkSetMaterialiser::Write(
        manifest,
        tree.Root() / "mod",
        tree.Root() / "chunks");

    REQUIRE(written.has_value());
    REQUIRE(*written == CHUNK_COUNT);

    const std::vector<std::uint8_t> firstChunk = ReadBytes(
        tree.Root() / "chunks" / (MakeDigest('a').ToHex() + ".chunk"));

    REQUIRE(firstChunk.size() == CHUNK_LENGTH);
    REQUIRE(std::equal(firstChunk.begin(), firstChunk.end(), content.begin()));
}

TEST_CASE("torrent describes every chunk file") {
    const TemporaryTree tree("torrent");
    const ModManifest manifest = MakeManifest();

    WriteBytes(tree.Root() / "mod" / "packs" / "ZZ_Win.dat", MakePattern(CHUNK_COUNT * CHUNK_LENGTH, 5));
    REQUIRE(ChunkSetMaterialiser::Write(manifest, tree.Root() / "mod", tree.Root() / "chunks").has_value());

    const auto torrent = ChunkSetTorrent::Create(tree.Root() / "chunks", TEST_PIECE_BYTES);

    REQUIRE(torrent.has_value());
    REQUIRE(torrent->payloadFiles == CHUNK_COUNT);
    REQUIRE(torrent->payloadBytes == CHUNK_COUNT * CHUNK_LENGTH);
    REQUIRE(torrent->infoHash.size() == 64);
    REQUIRE_FALSE(torrent->bencoded.empty());
}

TEST_CASE("piece alignment adds one pad entry per chunk") {
    const TemporaryTree tree("padding");
    const ModManifest manifest = MakeManifest();

    WriteBytes(tree.Root() / "mod" / "packs" / "ZZ_Win.dat", MakePattern(CHUNK_COUNT * CHUNK_LENGTH, 7));
    REQUIRE(ChunkSetMaterialiser::Write(manifest, tree.Root() / "mod", tree.Root() / "chunks").has_value());

    const auto torrent = ChunkSetTorrent::Create(tree.Root() / "chunks", TEST_PIECE_BYTES);

    REQUIRE(torrent.has_value());
    REQUIRE(torrent->padFiles == CHUNK_COUNT);
    REQUIRE(torrent->payloadBytes == CHUNK_COUNT * CHUNK_LENGTH);
}

TEST_CASE("torrent build refuses an empty folder") {
    const TemporaryTree tree("empty");

    std::error_code failure;
    std::filesystem::create_directories(tree.Root() / "chunks", failure);

    const auto torrent = ChunkSetTorrent::Create(tree.Root() / "chunks", TEST_PIECE_BYTES);

    REQUIRE_FALSE(torrent.has_value());
    REQUIRE(torrent.error() == wgrd::downloader::TorrentBuildError::FolderEmpty);
}

TEST_CASE("chunk set transfers between two loopback sessions") {
    const TemporaryTree seederTree("seeder");
    const TemporaryTree leecherTree("leecher");

    const ModManifest manifest = MakeManifest();
    const std::vector<std::uint8_t> content = MakePattern(CHUNK_COUNT * CHUNK_LENGTH, 23);

    WriteBytes(seederTree.Root() / "mod" / "packs" / "ZZ_Win.dat", content);
    REQUIRE(ChunkSetMaterialiser::Write(
        manifest,
        seederTree.Root() / "mod",
        seederTree.Root() / "chunks").has_value());

    const auto torrent = ChunkSetTorrent::Create(seederTree.Root() / "chunks", TEST_PIECE_BYTES);
    REQUIRE(torrent.has_value());

    SwarmNode seeder(seederTree.Root());
    REQUIRE(seeder.ListenPort() != 0);
    REQUIRE(seeder.Load(torrent->bencoded, true).has_value());

    SwarmNode leecher(leecherTree.Root());
    REQUIRE(leecher.ListenPort() != 0);
    REQUIRE(leecher.Load(torrent->bencoded, false).has_value());

    leecher.ConnectLoopbackPeer(seeder.ListenPort());

    REQUIRE(leecher.WaitForCompletion(std::chrono::seconds(60), &seeder, seeder.ListenPort()));

    const std::filesystem::path received = leecherTree.Root() / "chunks";
    REQUIRE(std::filesystem::is_directory(received));

    std::size_t receivedFiles = 0;
    for (const auto& entry : std::filesystem::directory_iterator(received)) {
        if (entry.is_regular_file()) {
            ++receivedFiles;
        }
    }
    REQUIRE(receivedFiles == CHUNK_COUNT);

    ChunkFolderSource source(received);
    const auto fetched = source.Fetch(MakeDigest('a'), CHUNK_LENGTH);

    REQUIRE(fetched.has_value());
    REQUIRE(fetched->size() == CHUNK_LENGTH);

    for (std::size_t position = 0; position < CHUNK_LENGTH; ++position) {
        REQUIRE(static_cast<std::uint8_t>((*fetched)[position]) == content[position]);
    }
}
