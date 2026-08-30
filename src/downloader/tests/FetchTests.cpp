#include "downloader/torrent/build/ChunkSetTorrentBuilder.h"
#include "downloader/transfer/TorrentSession.h"

#include "domain/types/content/ChunkDestination.h"
#include "domain/types/content/ChunkFileNaming.h"
#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkFileNaming;
using wgrd::domain::FetchPhase;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::ChunkSetTorrentBuilder;
using wgrd::downloader::TorrentSession;

namespace {
constexpr std::string_view LOOPBACK = "127.0.0.1:0";

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-fetch" / label;

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
	const auto digest = ChunkDigest::FromHex(std::format("{:064x}", index + 1));
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

ModManifest BuildNamedManifest(const std::vector<std::uint32_t>& lengths, const std::string& modName) {
	std::vector<ManifestChunk> chunks;
	std::uint64_t offset = 0;

	for (std::size_t index = 0; index < lengths.size(); ++index) {
		chunks.push_back(ManifestChunk{DigestFor(index), offset, lengths[index]});
		offset += lengths[index];
	}

	ManifestFile file{"packs/ZZ_Win.dat", offset, std::move(chunks)};

	return ModManifest(PublisherFingerprint{}, modName, 1, {file});
}

void WriteFilled(const std::filesystem::path& target, const std::uint64_t bytes, const char fill) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(fill);
	}
}

std::vector<char> ReadWhole(const std::filesystem::path& source) {
	std::ifstream input(source, std::ios::binary);

	return std::vector<char>(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);
}

void WriteSealed(const std::filesystem::path& target) {
	const std::vector<std::uint8_t> sealed(512, 0x5A);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(sealed.data()), static_cast<std::streamsize>(sealed.size()));
}

void WritePayload(const std::filesystem::path& target, const std::uint64_t bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(static_cast<char>((position * 29 + 7) & 0xFF));
	}
}

std::uint16_t AwaitPort(TorrentSession& session) {
	for (int attempt = 0; attempt < 400; ++attempt) {
		session.Poll();

		const std::uint16_t port = session.ListenPort();
		if (port != 0) {
			return port;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	return 0;
}
}

TEST_CASE("fetch pulls only the chunks that are missing") {
	const TemporaryTree seederTree("seed");
	const TemporaryTree leecherTree("leech");

	const std::vector<std::uint32_t> lengths = {20000, 50000, 16384, 65536};
	const ModManifest manifest = BuildManifest(lengths);

	const std::filesystem::path modFolder = seederTree.Root() / "mod";
	WritePayload(modFolder / "packs" / "ZZ_Win.dat", manifest.TotalBytes());

	const std::vector<std::uint8_t> sealed(512, 0x5A);
	const std::filesystem::path sealedPath = seederTree.Root() / "manifest.wgrdm";

	{
		std::ofstream output(sealedPath, std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<const char*>(sealed.data()), static_cast<std::streamsize>(sealed.size()));
	}

	const ChunkSetTorrentBuilder builder;
	const auto torrent = builder.Build(manifest, modFolder, sealed);
	REQUIRE(torrent.has_value());

	TorrentSession seeder(seederTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(seeder) != 0);
	REQUIRE(seeder.Announce(manifest, modFolder, sealedPath).has_value());

	TorrentSession leecher(leecherTree.Root(), std::string(LOOPBACK), false);
	const std::uint16_t leecherPort = AwaitPort(leecher);
	REQUIRE(leecherPort != 0);

	const std::vector<std::string> wanted = {
		ChunkFileNaming::FileNameFor(DigestFor(1)), ChunkFileNaming::FileNameFor(DigestFor(3))
	};

	const std::filesystem::path staging = leecherTree.Root() / "staging";

	REQUIRE(leecher.Begin("test/angel_maps", torrent->infoHash, staging, wanted, {}, false).has_value());

	leecher.ConnectLocalPeer(seeder.ListenPort());

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
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

	const wgrd::domain::FetchStatus status = leecher.Fetch();

	REQUIRE(status.phase == FetchPhase::Complete);
	REQUIRE(status.wantedBytes == lengths[1] + lengths[3]);

	const std::filesystem::path received =
			staging / (PublisherFingerprint{}.ToHex() + "_angel_maps");

	REQUIRE(std::filesystem::is_directory(received));

	std::ifstream payload(modFolder / "packs" / "ZZ_Win.dat", std::ios::binary);
	REQUIRE(payload);

	std::uint64_t offset = 0;
	for (std::size_t index = 0; index < lengths.size(); ++index) {
		const std::filesystem::path chunkFile = received / ChunkFileNaming::FileNameFor(DigestFor(index));
		const bool expected = index == 1 || index == 3;

		if (!expected) {
			REQUIRE_FALSE(std::filesystem::exists(chunkFile));
			offset += lengths[index];
			continue;
		}

		REQUIRE(std::filesystem::file_size(chunkFile) == lengths[index]);

		std::vector<char> source(lengths[index]);
		payload.seekg(static_cast<std::streamoff>(offset));
		payload.read(source.data(), lengths[index]);

		std::ifstream fetched(chunkFile, std::ios::binary);
		std::vector<char> actual(lengths[index]);
		fetched.read(actual.data(), lengths[index]);

		REQUIRE(actual == source);

		offset += lengths[index];
	}
}

TEST_CASE("fetch pulls the signed manifest before any chunks") {
	const TemporaryTree seederTree("seed-manifest");
	const TemporaryTree leecherTree("leech-manifest");

	const std::vector<std::uint32_t> lengths = {20000, 50000};
	const ModManifest manifest = BuildManifest(lengths);

	const std::filesystem::path modFolder = seederTree.Root() / "mod";
	WritePayload(modFolder / "packs" / "ZZ_Win.dat", manifest.TotalBytes());

	std::vector<std::uint8_t> sealed(1500);
	for (std::size_t index = 0; index < sealed.size(); ++index) {
		sealed[index] = static_cast<std::uint8_t>((index * 31 + 3) & 0xFF);
	}

	const std::filesystem::path sealedPath = seederTree.Root() / "manifest.wgrdm";
	{
		std::ofstream output(sealedPath, std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<const char*>(sealed.data()), static_cast<std::streamsize>(sealed.size()));
	}

	const ChunkSetTorrentBuilder builder;
	const auto torrent = builder.Build(manifest, modFolder, sealed);
	REQUIRE(torrent.has_value());

	TorrentSession seeder(seederTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(seeder) != 0);
	REQUIRE(seeder.Announce(manifest, modFolder, sealedPath).has_value());

	TorrentSession leecher(leecherTree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(leecher) != 0);

	const std::filesystem::path staging = leecherTree.Root() / "staging";

	const std::vector<std::string> wanted{std::string(ChunkFileNaming::MANIFEST_FILE)};

	REQUIRE(leecher.Begin("test/angel_maps", torrent->infoHash, staging, wanted, {}, false).has_value());

	leecher.ConnectLocalPeer(seeder.ListenPort());

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
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
	REQUIRE(leecher.Fetch().wantedBytes == sealed.size());

	const std::filesystem::path received = staging / manifest.TorrentName();

	const std::filesystem::path staged = received / std::string(ChunkFileNaming::MANIFEST_FILE);
	REQUIRE(std::filesystem::file_size(staged) == sealed.size());

	std::ifstream fetched(staged, std::ios::binary);
	std::vector<char> actual(sealed.size());
	fetched.read(actual.data(), static_cast<std::streamsize>(actual.size()));

	REQUIRE(std::equal(actual.begin(), actual.end(), reinterpret_cast<const char*>(sealed.data()))
	);

	for (std::size_t index = 0; index < lengths.size(); ++index) {
		REQUIRE_FALSE(std::filesystem::exists(
				received / ChunkFileNaming::FileNameFor(DigestFor(index)))
		);
	}
}

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
