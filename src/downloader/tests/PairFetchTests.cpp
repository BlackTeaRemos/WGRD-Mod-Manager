#include "downloader/torrent/build/VirtualChunkSetTorrent.h"
#include "downloader/transfer/TorrentSession.h"

#include "domain/types/content/ChunkFileNaming.h"
#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkFileNaming;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::TorrentSession;
using wgrd::downloader::VirtualChunkSetTorrent;

namespace {
constexpr std::chrono::milliseconds TICK{100};
constexpr std::size_t SEALED_MANIFEST_BYTES = 716;

const std::vector<std::uint32_t> CHUNK_LENGTHS = {20000, 50000, 16384, 65536};
constexpr std::uint32_t METADATA_BYTES = 41;

std::filesystem::path PairRoot(const std::string_view side) {
	return std::filesystem::temp_directory_path() / "wgrd-pair-fetch" / side;
}

ChunkDigest DigestFor(const std::size_t index) {
	const auto digest = ChunkDigest::FromHex(std::format("{:064x}", index + 1));
	REQUIRE(digest.has_value());
	return *digest;
}

ModManifest BuildManifest() {
	std::vector<ManifestChunk> chunks;
	std::uint64_t offset = 0;

	for (std::size_t index = 0; index < CHUNK_LENGTHS.size(); ++index) {
		chunks.push_back(ManifestChunk{DigestFor(index), offset, CHUNK_LENGTHS[index]});
		offset += CHUNK_LENGTHS[index];
	}

	ManifestFile payload{"pack.dat", offset, std::move(chunks)};

	std::vector<ManifestChunk> metadataChunks;
	metadataChunks.push_back(ManifestChunk{DigestFor(CHUNK_LENGTHS.size()), 0, METADATA_BYTES});

	ManifestFile metadata{"mod.json", METADATA_BYTES, std::move(metadataChunks)};

	return ModManifest(PublisherFingerprint{}, "pairmod", 1, {payload, metadata});
}

void WritePayload(const std::filesystem::path& target, const std::uint64_t bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(static_cast<char>((position * 37 + 11) & 0xFF));
	}
}

std::vector<std::uint8_t> SealedManifestBytes() {
	std::vector<std::uint8_t> sealed(SEALED_MANIFEST_BYTES);

	for (std::size_t index = 0; index < sealed.size(); ++index) {
		sealed[index] = static_cast<std::uint8_t>(((index * 31) ^ (index >> 8) ^ 0xA5) & 0xFF);
	}

	return sealed;
}

std::filesystem::path WriteSealedManifest(const std::filesystem::path& folder) {
	std::error_code failure;
	std::filesystem::create_directories(folder, failure);

	const std::filesystem::path target = folder / "sealed.wgrdm";
	const std::vector<std::uint8_t> sealed = SealedManifestBytes();

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(
		reinterpret_cast<const char*>(sealed.data()),
		static_cast<std::streamsize>(sealed.size())
	);

	return target;
}

std::string ReadRole() {
	std::size_t length = 0;
	char buffer[16] = {};
	getenv_s(&length, buffer, sizeof(buffer), "WGRD_PAIR_ROLE");

	return std::string(buffer);
}
}

TEST_CASE("probe reaches a running manager", "[.probe]") {
	std::size_t length = 0;
	char buffer[80] = {};
	getenv_s(&length, buffer, sizeof(buffer), "WGRD_PROBE_INFOHASH");

	const auto infoHash = ChunkDigest::FromHex(std::string(buffer));
	REQUIRE(infoHash.has_value());

	const std::filesystem::path root = PairRoot("probe");

	std::error_code failure;
	std::filesystem::remove_all(root, failure);
	std::filesystem::create_directories(root, failure);

	TorrentSession session(root / "swarm", std::string(TorrentSession::PUBLIC_INTERFACES), true);

	const auto begun = session.Begin(
		"probe/target",
		*infoHash,
		root / "incoming",
		std::vector<std::string>{std::string(ChunkFileNaming::MANIFEST_FILE)},
		{},
		false
	);

	REQUIRE(begun.has_value());

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);

	std::uint32_t bestPeers = 0;
	wgrd::domain::FetchStatus status;

	while (std::chrono::steady_clock::now() < deadline) {
		session.Poll();
		status = session.Fetch();

		bestPeers = status.peers > bestPeers ? status.peers : bestPeers;

		if (status.phase == wgrd::domain::FetchPhase::Complete) {
			break;
		}

		std::this_thread::sleep_for(TICK);
	}

	WARN("probe phase " << static_cast<int>(status.phase)
		<< " bestPeers " << bestPeers
		<< " fetched " << status.fetchedBytes
	);

	REQUIRE(bestPeers > 0);
}

TEST_CASE("one process fetches a chunk set from another copy", "[.pairfetch]") {
	const std::string role = ReadRole();
	const bool seeding = role == "seed";

	const std::filesystem::path root = PairRoot(seeding ? "seed" : "leech");

	std::error_code failure;
	std::filesystem::remove_all(root, failure);
	std::filesystem::create_directories(root, failure);

	const ModManifest manifest = BuildManifest();

	const std::filesystem::path modFolder = root / "mod";

	std::uint64_t payloadBytes = 0;
	for (const std::uint32_t length : CHUNK_LENGTHS) {
		payloadBytes += length;
	}

	WritePayload(modFolder / "pack.dat", payloadBytes);
	WritePayload(modFolder / "mod.json", METADATA_BYTES);

	const std::filesystem::path sealedPath = WriteSealedManifest(root / "manifests");

	const auto expected = VirtualChunkSetTorrent::Create(
		manifest,
		modFolder,
		manifest.TorrentName(),
		SealedManifestBytes()
	);

	REQUIRE(expected.has_value());

	TorrentSession session(root / "swarm", std::string(TorrentSession::PUBLIC_INTERFACES), true);

	if (seeding) {
		const auto seeded = session.Announce(manifest, modFolder, sealedPath);
		REQUIRE(seeded.has_value());
		REQUIRE(seeded->infoHash == expected->infoHash);

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
		while (std::chrono::steady_clock::now() < deadline) {
			session.Poll();
			std::this_thread::sleep_for(TICK);
		}

		const std::vector<wgrd::domain::SeedEntry>& entries = session.Entries();
		REQUIRE(entries.size() == 1);

		WARN("seed infohash " << entries[0].infoHash
			<< " seeding " << entries[0].seeding
			<< " peers " << entries[0].peers
			<< " uploaded " << entries[0].uploadedBytes
		);

		return;
	}

	std::vector<std::string> wanted;
	wanted.push_back(std::string(ChunkFileNaming::MANIFEST_FILE));
	for (std::size_t index = 0; index <= CHUNK_LENGTHS.size(); ++index) {
		wanted.push_back(ChunkFileNaming::FileNameFor(DigestFor(index)));
	}

	const std::filesystem::path staging = root / "incoming";

	const auto infoHash = ChunkDigest::FromHex(expected->infoHash);
	REQUIRE(infoHash.has_value());

	const auto begun = session.Begin(
		manifest.Identifier(),
		*infoHash,
		staging,
		wanted,
		{},
		false
	);

	REQUIRE(begun.has_value());

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(75);

	wgrd::domain::FetchStatus status;

	while (std::chrono::steady_clock::now() < deadline) {
		session.Poll();
		status = session.Fetch();

		if (status.phase == wgrd::domain::FetchPhase::Complete
		    || status.phase == wgrd::domain::FetchPhase::Failed) {
			break;
		}

		std::this_thread::sleep_for(TICK);
	}

	WARN("leech phase " << static_cast<int>(status.phase)
		<< " fetched " << status.fetchedBytes
		<< " peers " << status.peers
		<< " gossipPeers " << session.Gossip().peers
		<< " dials " << session.Gossip().neighbourDials
		<< " lastPeerError [" << session.Gossip().lastPeerError << "]"
	);

	REQUIRE(status.phase == wgrd::domain::FetchPhase::Complete);

	const std::filesystem::path received = staging / manifest.TorrentName();

	REQUIRE(std::filesystem::is_directory(received));

	REQUIRE(
		std::filesystem::file_size(received / std::string(ChunkFileNaming::MANIFEST_FILE))
		== SEALED_MANIFEST_BYTES
	);

	for (std::size_t index = 0; index < CHUNK_LENGTHS.size(); ++index) {
		const std::filesystem::path chunkFile =
				received / ChunkFileNaming::FileNameFor(DigestFor(index));

		REQUIRE(std::filesystem::file_size(chunkFile) == CHUNK_LENGTHS[index]);
	}

	REQUIRE(
		std::filesystem::file_size(received / ChunkFileNaming::FileNameFor(DigestFor(CHUNK_LENGTHS.size())))
		== METADATA_BYTES
	);
}
