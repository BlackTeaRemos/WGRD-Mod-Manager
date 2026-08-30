#include "downloader/storage/ChunkLocator.h"
#include "downloader/torrent/build/ChunkMerkleHasher.h"
#include "downloader/torrent/build/ChunkSetTorrent.h"
#include "downloader/torrent/build/VirtualChunkSetTorrent.h"
#include "downloader/torrent/chunkset/ChunkSetMaterialiser.h"
#include "downloader/transfer/SwarmNode.h"

#include "domain/types/content/ChunkFileNaming.h"
#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::ChunkMerkleHasher;
using wgrd::downloader::ChunkSetMaterialiser;
using wgrd::downloader::ChunkSetTorrent;
using wgrd::downloader::VirtualChunkSetTorrent;

namespace {
constexpr std::int32_t PIECE_BYTES = 32768;
constexpr std::string_view TORRENT_NAME = "chunks";

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-virtual" / label;

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

ChunkDigest DigestFor(const std::size_t index) {
	const std::string hex = std::format("{:064x}", index + 1);
	const auto digest = ChunkDigest::FromHex(hex);
	REQUIRE(digest.has_value());
	return *digest;
}

ModManifest BuildManifest(const std::vector<std::uint32_t>& lengths) {
	std::vector<ManifestChunk> chunks;
	std::uint64_t offset = 0;

	for (std::size_t index = 0; index < lengths.size(); ++index) {
		chunks.push_back(ManifestChunk{DigestFor(index), offset, lengths[index]});
		offset += lengths[index];
	}

	ManifestFile file{"packs/ZZ_Win.dat", offset, std::move(chunks)};

	return ModManifest(PublisherFingerprint{}, "angel_maps", 1, {file});
}

void WritePayload(const std::filesystem::path& target, const std::uint64_t bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(static_cast<char>((position * 37 + 11) & 0xFF));
	}
}

std::vector<std::uint8_t> MakeSealedManifest(const std::size_t bytes) {
	std::vector<std::uint8_t> sealed(bytes);

	for (std::size_t index = 0; index < bytes; ++index) {
		sealed[index] = static_cast<std::uint8_t>(((index * 31) ^ (index >> 8) ^ 0xA5) & 0xFF);
	}

	return sealed;
}

void RequireIdenticalTorrent(
	const std::vector<std::uint32_t>& lengths,
	std::size_t sealedManifestBytes
) {
	const TemporaryTree tree("compare");
	const ModManifest manifest = BuildManifest(lengths);

	WritePayload(tree.Root() / "mod" / "packs" / "ZZ_Win.dat", manifest.TotalBytes());

	REQUIRE(ChunkSetMaterialiser::Write(
			manifest,
			tree.Root() / "mod",
			tree.Root() / TORRENT_NAME).has_value()
	);

	const std::vector<std::uint8_t> sealed = MakeSealedManifest(sealedManifestBytes);

	if (!sealed.empty()) {
		std::ofstream output(
			tree.Root() / TORRENT_NAME / std::string(wgrd::domain::ChunkFileNaming::MANIFEST_FILE),
			std::ios::binary | std::ios::trunc
		);

		output.write(
			reinterpret_cast<const char*>(sealed.data()),
			static_cast<std::streamsize>(sealed.size())
		);
	}

	const auto materialised = ChunkSetTorrent::Create(tree.Root() / TORRENT_NAME, PIECE_BYTES);
	const auto virtualised = VirtualChunkSetTorrent::Create(
		manifest,
		tree.Root() / "mod",
		TORRENT_NAME,
		sealed,
		PIECE_BYTES
	);

	REQUIRE(materialised.has_value());
	REQUIRE(virtualised.has_value());

	REQUIRE(virtualised->payloadFiles == materialised->payloadFiles);
	REQUIRE(virtualised->payloadBytes == materialised->payloadBytes);
	REQUIRE(virtualised->infoHash == materialised->infoHash);
}

void RequireIdenticalTorrent(const std::vector<std::uint32_t>& lengths) {
	RequireIdenticalTorrent(lengths, 0);
}
}

TEST_CASE("leaf target pads a sub piece file to a power of two") {
	REQUIRE(ChunkMerkleHasher::LeafTarget(100, PIECE_BYTES) == 1);
	REQUIRE(ChunkMerkleHasher::LeafTarget(16384, PIECE_BYTES) == 1);
	REQUIRE(ChunkMerkleHasher::LeafTarget(16385, PIECE_BYTES) == 2);
	REQUIRE(ChunkMerkleHasher::LeafTarget(32768, PIECE_BYTES) == 2);
	REQUIRE(ChunkMerkleHasher::LeafTarget(65536, PIECE_BYTES) == 2);
}

TEST_CASE("block count rounds up") {
	REQUIRE(ChunkMerkleHasher::BlockCount(0) == 0);
	REQUIRE(ChunkMerkleHasher::BlockCount(1) == 1);
	REQUIRE(ChunkMerkleHasher::BlockCount(16384) == 1);
	REQUIRE(ChunkMerkleHasher::BlockCount(16385) == 2);
}

TEST_CASE("virtual torrent matches a materialised one for sub block chunks") {
	RequireIdenticalTorrent({100, 512, 4096});
}

TEST_CASE("virtual torrent matches a materialised one at block boundaries") {
	RequireIdenticalTorrent({16384, 32768});
}

TEST_CASE("virtual torrent matches a materialised one for partial final blocks") {
	RequireIdenticalTorrent({20000, 50000});
}

TEST_CASE("virtual torrent matches a materialised one across multiple pieces") {
	RequireIdenticalTorrent({65536, 98304, 131072});
}

TEST_CASE("virtual torrent matches a materialised one for a mixed chunk set") {
	RequireIdenticalTorrent({100, 16384, 20000, 32768, 50000, 65536});
}

TEST_CASE("seeder serves chunks straight from the installed mod folder") {
	const TemporaryTree seederTree("seed-virtual");
	const TemporaryTree leecherTree("leech-virtual");

	const std::vector<std::uint32_t> lengths = {20000, 50000, 16384, 65536};
	const ModManifest manifest = BuildManifest(lengths);

	const std::filesystem::path modFolder = seederTree.Root() / "mod";
	WritePayload(modFolder / "packs" / "ZZ_Win.dat", manifest.TotalBytes());

	wgrd::downloader::ChunkLocator locator;
	locator.Register(manifest, modFolder);
	REQUIRE(locator.Count() == lengths.size());

	SECTION("locator keeps every destination of a repeated chunk") {
		wgrd::downloader::ChunkLocator repeated;

		const std::string name = "repeated.chunk";

		repeated.RegisterDestination(name, modFolder / "a.dat", 0, 64);
		repeated.RegisterDestination(name, modFolder / "a.dat", 4096, 64);
		repeated.RegisterDestination(name, modFolder / "b.dat", 128, 64);
		repeated.RegisterDestination(name, modFolder / "a.dat", 0, 64);

		REQUIRE(repeated.DestinationCount() == 1);
		REQUIRE(repeated.FindDestinations(name).size() == 3);
		REQUIRE(repeated.HasDestination(name));

		repeated.ClearDestinations();

		REQUIRE(repeated.FindDestinations(name).empty());
		REQUIRE_FALSE(repeated.HasDestination(name));
	}

	SECTION("destinations never reach seeded placements") {
		wgrd::downloader::ChunkLocator shared;

		const std::string name = "shared.chunk";

		shared.RegisterFile(name, modFolder / "installed.dat", 512, 64);
		shared.RegisterDestination(name, modFolder / "target.dat", 128, 64);

		REQUIRE(shared.FindDestinations(name).size() == 1);
		REQUIRE(shared.FindDestinations(name).front().file == modFolder / "target.dat");

		const auto seeded = shared.Find(name);

		REQUIRE(seeded.has_value());
		REQUIRE(seeded->file == modFolder / "installed.dat");

		shared.ClearDestinations();

		REQUIRE(shared.Find(name).has_value());
		REQUIRE(shared.Count() == 1);
	}

	SECTION("forgetting one mod keeps another sharing the chunk") {
		wgrd::downloader::ChunkLocator shared;

		const std::filesystem::path other = seederTree.Root() / "other";

		shared.Register(manifest, modFolder);
		shared.Register(manifest, other);

		const std::string name = wgrd::domain::ChunkFileNaming::FileNameFor(
			manifest.Files().front().chunks.front().digest
		);

		REQUIRE(shared.Count() == lengths.size());

		shared.Forget(manifest, other);

		const auto surviving = shared.Find(name);

		REQUIRE(surviving.has_value());
		REQUIRE(surviving->file == modFolder / manifest.Files().front().path);

		shared.Forget(manifest, modFolder);

		REQUIRE(shared.Count() == 0);
	}

	const auto torrent = VirtualChunkSetTorrent::Create(
		manifest,
		modFolder,
		TORRENT_NAME,
		{},
		PIECE_BYTES
	);

	REQUIRE(torrent.has_value());

	wgrd::downloader::SwarmNode seeder(seederTree.Root(), locator);
	REQUIRE(seeder.ListenPort() != 0);
	REQUIRE(seeder.Load(torrent->bencoded, false).has_value());

	wgrd::downloader::ChunkLocator leecherLocator;
	wgrd::downloader::SwarmNode leecher(leecherTree.Root(), leecherLocator);
	REQUIRE(leecher.ListenPort() != 0);
	REQUIRE(leecher.Load(torrent->bencoded, false).has_value());

	leecher.ConnectLoopbackPeer(seeder.ListenPort());

	const bool completed = leecher.WaitForCompletion(std::chrono::seconds(60), &seeder, seeder.ListenPort());
	seeder.Pump();
	if (!completed) {
		for (const std::string& message : seeder.Messages()) {
			UNSCOPED_INFO("seeder " << message);
		}
		for (const std::string& message : leecher.Messages()) {
			UNSCOPED_INFO("leecher " << message);
		}
	}
	REQUIRE(completed);

	REQUIRE_FALSE(std::filesystem::exists(seederTree.Root() / TORRENT_NAME));

	const std::filesystem::path received = leecherTree.Root() / TORRENT_NAME;
	REQUIRE(std::filesystem::is_directory(received));

	std::ifstream payload(modFolder / "packs" / "ZZ_Win.dat", std::ios::binary);
	REQUIRE(payload);

	std::uint64_t offset = 0;
	for (std::size_t index = 0; index < lengths.size(); ++index) {
		const std::filesystem::path chunkFile =
				received / (DigestFor(index).ToHex() + ".chunk");

		REQUIRE(std::filesystem::file_size(chunkFile) == lengths[index]);

		std::vector<char> expected(lengths[index]);
		payload.seekg(static_cast<std::streamoff>(offset));
		payload.read(expected.data(), lengths[index]);

		std::ifstream actualStream(chunkFile, std::ios::binary);
		std::vector<char> actual(lengths[index]);
		actualStream.read(actual.data(), lengths[index]);

		REQUIRE(actual == expected);

		offset += lengths[index];
	}
}

TEST_CASE("virtual torrent matches a materialised one carrying a sub block manifest") {
	RequireIdenticalTorrent({20000, 50000}, 1500);
}

TEST_CASE("virtual torrent matches a materialised one carrying a multi block manifest") {
	RequireIdenticalTorrent({20000, 50000}, 20000);
}

TEST_CASE("virtual torrent matches a materialised one carrying a multi piece manifest") {
	RequireIdenticalTorrent({16384, 32768}, 70000);
}

TEST_CASE("virtual torrent matches a materialised one carrying an exact block manifest") {
	RequireIdenticalTorrent({20000}, 16384);
}
