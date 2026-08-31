#include "downloader/tests/FetchTestSupport.h"

#include "downloader/torrent/build/ChunkSetTorrentBuilder.h"

#include "domain/types/content/ChunkDestination.h"
#include "domain/types/content/ChunkFileNaming.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using wgrd::domain::ChunkFileNaming;
using wgrd::domain::FetchPhase;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::ChunkSetTorrentBuilder;
using wgrd::downloader::TorrentSession;
using wgrd::downloader::tests::AwaitPort;
using wgrd::downloader::tests::BuildManifest;
using wgrd::downloader::tests::BuildNamedManifest;
using wgrd::downloader::tests::DigestFor;
using wgrd::downloader::tests::LOOPBACK;
using wgrd::downloader::tests::ReadWhole;
using wgrd::downloader::tests::TemporaryTree;
using wgrd::downloader::tests::WriteFilled;
using wgrd::downloader::tests::WritePayload;
using wgrd::downloader::tests::WriteSealed;

TEST_CASE("fetch writes destinations without touching a mod that shares chunks") {
	const TemporaryTree seederTree("seed-destinations");
	const TemporaryTree leecherTree("leech-destinations");

	const std::vector<std::uint32_t> lengths = {20000, 50000, 16384, 65536};
	const ModManifest manifest = BuildManifest(lengths);
	const std::uint64_t totalBytes = manifest.TotalBytes();

	const std::filesystem::path modFolder = seederTree.Root() / "mod";
	WritePayload(modFolder / "packs" / "ZZ_Win.dat", totalBytes);

	const std::filesystem::path sealedPath = seederTree.Root() / "manifest.wgrdm";
	WriteSealed(sealedPath);

	const std::vector<std::uint8_t> sealed(512, 0x5A);

	const ChunkSetTorrentBuilder builder;
	const auto torrent = builder.Build(manifest, modFolder, sealed);
	REQUIRE(torrent.has_value());

	TorrentSession seeder(seederTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(seeder) != 0);
	REQUIRE(seeder.Announce(manifest, modFolder, sealedPath).has_value());

	TorrentSession leecher(leecherTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(leecher) != 0);

	const ModManifest neighbour = BuildNamedManifest(lengths, "other_maps");
	const std::filesystem::path neighbourFolder = leecherTree.Root() / "other";
	const std::filesystem::path neighbourFile = neighbourFolder / "packs" / "ZZ_Win.dat";

	WriteFilled(neighbourFile, totalBytes, static_cast<char>(0x7E));

	const std::filesystem::path neighbourSealed = leecherTree.Root() / "other.wgrdm";
	WriteSealed(neighbourSealed);

	REQUIRE(leecher.Announce(neighbour, neighbourFolder, neighbourSealed).has_value());

	const std::vector<char> neighbourBefore = ReadWhole(neighbourFile);

	const std::filesystem::path targetFile =
			leecherTree.Root() / "target" / "packs" / "ZZ_Win.dat";

	WriteFilled(targetFile, totalBytes, char{0});

	std::vector<std::string> wanted;
	std::vector<wgrd::domain::ChunkDestination> destinations;

	for (std::size_t index = 0; index < lengths.size(); ++index) {
		const std::string chunkFileName = ChunkFileNaming::FileNameFor(DigestFor(index));

		wanted.push_back(chunkFileName);

		destinations.push_back(wgrd::domain::ChunkDestination{
				chunkFileName,
				targetFile,
				manifest.Files().front().chunks[index].offset,
				lengths[index]
			}
		);
	}

	const std::filesystem::path staging = leecherTree.Root() / "staging";

	REQUIRE(leecher.Begin("test/angel_maps", torrent->infoHash, staging, wanted, destinations, false)
		.has_value());

	leecher.ConnectLocalPeer(seeder.ListenPort());

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
	auto nextDial = std::chrono::steady_clock::now() + std::chrono::seconds(2);

	while (std::chrono::steady_clock::now() < deadline) {
		seeder.Poll();
		leecher.Poll();

		if (std::chrono::steady_clock::now() >= nextDial) {
			leecher.ConnectLocalPeer(seeder.ListenPort());
			nextDial = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		}

		if (leecher.Fetch().phase == FetchPhase::Complete) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	REQUIRE(leecher.Fetch().phase == FetchPhase::Complete);

	const std::vector<char> expected = ReadWhole(modFolder / "packs" / "ZZ_Win.dat");
	const std::vector<char> received = ReadWhole(targetFile);

	REQUIRE(received.size() == expected.size());
	REQUIRE(received == expected);

	const std::vector<char> neighbourAfter = ReadWhole(neighbourFile);

	REQUIRE(neighbourAfter == neighbourBefore);

	for (std::size_t index = 0; index < lengths.size(); ++index) {
		const std::filesystem::path chunkFile =
				staging / (PublisherFingerprint{}.ToHex() + "_angel_maps")
				/ ChunkFileNaming::FileNameFor(DigestFor(index));

		REQUIRE_FALSE(std::filesystem::exists(chunkFile));
	}
}

TEST_CASE("fresh fetch fills every placement of a repeated chunk") {
	const TemporaryTree seederTree("seed-repeated");
	const TemporaryTree leecherTree("leech-repeated");

	const std::vector<std::uint32_t> distinctLengths = {20000, 50000, 16384};
	const std::vector<std::size_t> sequence = {0, 1, 0, 2, 1, 0};

	std::vector<ManifestChunk> chunks;
	std::vector<char> expected;
	std::uint64_t offset = 0;

	for (const std::size_t which : sequence) {
		const std::uint32_t length = distinctLengths[which];

		for (std::uint32_t position = 0; position < length; ++position) {
			expected.push_back(static_cast<char>((position * 31 + which * 97 + 11) & 0xFF));
		}

		chunks.push_back(ManifestChunk{DigestFor(which), offset, length});
		offset += length;
	}

	ManifestFile file{"packs/ZZ_Win.dat", offset, chunks};
	const ModManifest manifest(PublisherFingerprint{}, "angel_maps", 1, {file});

	const std::filesystem::path modFolder = seederTree.Root() / "mod";
	const std::filesystem::path payloadPath = modFolder / "packs" / "ZZ_Win.dat";

	{
		std::error_code failure;
		std::filesystem::create_directories(payloadPath.parent_path(), failure);

		std::ofstream output(payloadPath, std::ios::binary | std::ios::trunc);
		output.write(expected.data(), static_cast<std::streamsize>(expected.size()));
	}

	const std::filesystem::path sealedPath = seederTree.Root() / "manifest.wgrdm";
	WriteSealed(sealedPath);

	const std::vector<std::uint8_t> sealed(512, 0x5A);

	const ChunkSetTorrentBuilder builder;
	const auto torrent = builder.Build(manifest, modFolder, sealed);
	REQUIRE(torrent.has_value());

	TorrentSession seeder(seederTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(seeder) != 0);
	REQUIRE(seeder.Announce(manifest, modFolder, sealedPath).has_value());

	TorrentSession leecher(leecherTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(leecher) != 0);

	const std::filesystem::path targetFile =
			leecherTree.Root() / "target" / "packs" / "ZZ_Win.dat";

	WriteFilled(targetFile, expected.size(), char{0});

	std::vector<std::string> wanted;
	std::vector<wgrd::domain::ChunkDestination> destinations;
	std::vector<std::string> seen;

	for (const ManifestChunk& chunk : chunks) {
		const std::string chunkFileName = ChunkFileNaming::FileNameFor(chunk.digest);

		if (std::ranges::find(seen, chunkFileName) == seen.end()) {
			seen.push_back(chunkFileName);
			wanted.push_back(chunkFileName);
		}

		destinations.push_back(wgrd::domain::ChunkDestination{
				chunkFileName, targetFile, chunk.offset, chunk.length
			}
		);
	}

	const std::filesystem::path staging = leecherTree.Root() / "staging";

	REQUIRE(leecher.Begin("test/angel_maps", torrent->infoHash, staging, wanted, destinations, false)
		.has_value());

	leecher.ConnectLocalPeer(seeder.ListenPort());

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
	auto nextDial = std::chrono::steady_clock::now() + std::chrono::seconds(2);

	while (std::chrono::steady_clock::now() < deadline) {
		seeder.Poll();
		leecher.Poll();

		if (std::chrono::steady_clock::now() >= nextDial) {
			leecher.ConnectLocalPeer(seeder.ListenPort());
			nextDial = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		}

		if (leecher.Fetch().phase == FetchPhase::Complete) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	REQUIRE(leecher.Fetch().phase == FetchPhase::Complete);

	const std::vector<char> received = ReadWhole(targetFile);

	REQUIRE(received.size() == expected.size());

	std::uint64_t placementOffset = 0;
	for (const ManifestChunk& chunk : chunks) {
		const std::vector<char> slice(
			received.begin() + static_cast<std::ptrdiff_t>(placementOffset),
			received.begin() + static_cast<std::ptrdiff_t>(placementOffset + chunk.length)
		);

		const std::vector<char> wantedSlice(
			expected.begin() + static_cast<std::ptrdiff_t>(placementOffset),
			expected.begin() + static_cast<std::ptrdiff_t>(placementOffset + chunk.length)
		);

		REQUIRE(slice == wantedSlice);

		placementOffset += chunk.length;
	}

	REQUIRE(received == expected);
}

TEST_CASE("fresh fetch reproduces a multi file mod with uneven chunks") {
	const TemporaryTree seederTree("seed-multifile");
	const TemporaryTree leecherTree("leech-multifile");

	struct PlannedFile {
		std::string path;
		std::vector<std::size_t> sequence;
	};

	const std::vector<std::uint32_t> distinctLengths = {7000, 13, 65537, 1, 40000, 16384};

	const std::vector<PlannedFile> planned = {
		{"packs/ZZ_Win.dat", {0, 1, 2, 3}},
		{"packs/ZZ_Data.dat", {4, 0, 5, 1}},
		{"mod.json", {3, 2}}
	};

	auto BodyFor = [&distinctLengths](const std::size_t which) {
		std::vector<char> body;
		for (std::uint32_t position = 0; position < distinctLengths[which]; ++position) {
			body.push_back(static_cast<char>((position * 17 + which * 53 + 3) & 0xFF));
		}
		return body;
	};

	std::vector<ManifestFile> manifestFiles;
	std::vector<std::vector<char>> originals;

	const std::filesystem::path modFolder = seederTree.Root() / "mod";

	for (const PlannedFile& entry : planned) {
		std::vector<ManifestChunk> chunks;
		std::vector<char> content;
		std::uint64_t offset = 0;

		for (const std::size_t which : entry.sequence) {
			const std::vector<char> body = BodyFor(which);
			content.insert(content.end(), body.begin(), body.end());

			chunks.push_back(ManifestChunk{DigestFor(which), offset, distinctLengths[which]});
			offset += distinctLengths[which];
		}

		const std::filesystem::path payloadPath = modFolder / entry.path;

		std::error_code failure;
		std::filesystem::create_directories(payloadPath.parent_path(), failure);

		std::ofstream output(payloadPath, std::ios::binary | std::ios::trunc);
		output.write(content.data(), static_cast<std::streamsize>(content.size()));
		output.close();

		manifestFiles.push_back(ManifestFile{entry.path, offset, std::move(chunks)});
		originals.push_back(std::move(content));
	}

	const ModManifest manifest(PublisherFingerprint{}, "angel_maps", 1, manifestFiles);

	const std::filesystem::path sealedPath = seederTree.Root() / "manifest.wgrdm";
	WriteSealed(sealedPath);

	const std::vector<std::uint8_t> sealed(512, 0x5A);

	const ChunkSetTorrentBuilder builder;
	const auto torrent = builder.Build(manifest, modFolder, sealed);
	REQUIRE(torrent.has_value());

	TorrentSession seeder(seederTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(seeder) != 0);
	REQUIRE(seeder.Announce(manifest, modFolder, sealedPath).has_value());

	TorrentSession leecher(leecherTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(leecher) != 0);

	const std::filesystem::path targetFolder = leecherTree.Root() / "target";

	std::vector<std::string> wanted;
	std::vector<std::string> seen;
	std::vector<wgrd::domain::ChunkDestination> destinations;
	std::vector<std::filesystem::path> targets;

	for (const ManifestFile& file : manifest.Files()) {
		const std::filesystem::path staged = targetFolder / file.path;

		WriteFilled(staged, file.size, char{0});
		targets.push_back(staged);

		for (const ManifestChunk& chunk : file.chunks) {
			const std::string chunkFileName = ChunkFileNaming::FileNameFor(chunk.digest);

			if (std::ranges::find(seen, chunkFileName) == seen.end()) {
				seen.push_back(chunkFileName);
				wanted.push_back(chunkFileName);
			}

			destinations.push_back(wgrd::domain::ChunkDestination{
					chunkFileName, staged, chunk.offset, chunk.length
				}
			);
		}
	}

	const std::filesystem::path staging = leecherTree.Root() / "staging";

	REQUIRE(leecher.Begin("test/angel_maps", torrent->infoHash, staging, wanted, destinations, false)
		.has_value());

	leecher.ConnectLocalPeer(seeder.ListenPort());

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
	auto nextDial = std::chrono::steady_clock::now() + std::chrono::seconds(2);

	while (std::chrono::steady_clock::now() < deadline) {
		seeder.Poll();
		leecher.Poll();

		if (std::chrono::steady_clock::now() >= nextDial) {
			leecher.ConnectLocalPeer(seeder.ListenPort());
			nextDial = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		}

		if (leecher.Fetch().phase == FetchPhase::Complete) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	REQUIRE(leecher.Fetch().phase == FetchPhase::Complete);

	for (std::size_t index = 0; index < targets.size(); ++index) {
		const std::vector<char> received = ReadWhole(targets[index]);

		REQUIRE(received.size() == originals[index].size());
		REQUIRE(received == originals[index]);
	}
}
