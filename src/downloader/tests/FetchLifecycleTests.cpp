#include "downloader/tests/FetchTestSupport.h"

#include "downloader/torrent/build/ChunkSetTorrentBuilder.h"

#include "domain/types/content/ChunkDestination.h"
#include "domain/types/content/ChunkFileNaming.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using wgrd::domain::ChunkFileNaming;
using wgrd::domain::FetchError;
using wgrd::domain::FetchPhase;
using wgrd::domain::ModManifest;
using wgrd::downloader::ChunkSetTorrentBuilder;
using wgrd::downloader::TorrentSession;
using wgrd::downloader::tests::AwaitPort;
using wgrd::downloader::tests::BuildManifest;
using wgrd::downloader::tests::DigestFor;
using wgrd::downloader::tests::LOOPBACK;
using wgrd::downloader::tests::TemporaryTree;
using wgrd::downloader::tests::WriteFilled;
using wgrd::downloader::tests::WritePayload;
using wgrd::downloader::tests::WriteSealed;

TEST_CASE("begin refuses an infohash already seeded") {
	const TemporaryTree tree("begin-duplicate");

	const std::vector<std::uint32_t> lengths = {20000, 50000};
	const ModManifest manifest = BuildManifest(lengths);

	const std::filesystem::path modFolder = tree.Root() / "mod";
	WritePayload(modFolder / "packs" / "ZZ_Win.dat", manifest.TotalBytes());

	const std::filesystem::path sealedPath = tree.Root() / "manifest.wgrdm";
	WriteSealed(sealedPath);

	const std::vector<std::uint8_t> sealed(512, 0x5A);

	const ChunkSetTorrentBuilder builder;
	const auto torrent = builder.Build(manifest, modFolder, sealed);
	REQUIRE(torrent.has_value());

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(session) != 0);
	REQUIRE(session.Announce(manifest, modFolder, sealedPath).has_value());

	const std::vector<std::string> wanted{ChunkFileNaming::FileNameFor(DigestFor(0))};

	const auto begun = session.Begin(
		"test/angel_maps",
		torrent->infoHash,
		tree.Root() / "staging",
		wanted,
		{},
		false
	);

	REQUIRE_FALSE(begun.has_value());
	REQUIRE(begun.error() == FetchError::AlreadyPresent);
	REQUIRE(session.Fetch().phase == FetchPhase::Idle);
}

TEST_CASE("failed announce leaves no chunk registrations") {
	const TemporaryTree tree("announce-rollback");

	const std::vector<std::uint32_t> lengths = {20000, 50000};
	const ModManifest manifest = BuildManifest(lengths);

	const std::filesystem::path modFolder = tree.Root() / "mod";
	WritePayload(modFolder / "packs" / "ZZ_Win.dat", manifest.TotalBytes());

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);

	SECTION("sealed manifest missing") {
		const auto announced =
				session.Announce(manifest, modFolder, tree.Root() / "absent.wgrdm");

		REQUIRE_FALSE(announced.has_value());
		REQUIRE(session.RegisteredChunkFiles() == 0);
	}

	SECTION("mod payload missing") {
		const std::filesystem::path sealedPath = tree.Root() / "manifest.wgrdm";
		WriteSealed(sealedPath);

		const auto announced =
				session.Announce(manifest, tree.Root() / "missing-mod", sealedPath);

		REQUIRE_FALSE(announced.has_value());
		REQUIRE(session.RegisteredChunkFiles() == 0);
	}
}

TEST_CASE("fetch fails when a destination cannot hold a chunk") {
	const TemporaryTree seederTree("seed-write-fail");
	const TemporaryTree leecherTree("leech-write-fail");

	const std::vector<std::uint32_t> lengths = {20000, 50000};
	const ModManifest manifest = BuildManifest(lengths);

	const std::filesystem::path modFolder = seederTree.Root() / "mod";
	WritePayload(modFolder / "packs" / "ZZ_Win.dat", manifest.TotalBytes());

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

	WriteFilled(targetFile, 100, char{0});

	const std::string chunkFileName = ChunkFileNaming::FileNameFor(DigestFor(0));

	const std::vector<std::string> wanted{chunkFileName};

	const std::vector<wgrd::domain::ChunkDestination> destinations{
		wgrd::domain::ChunkDestination{chunkFileName, targetFile, 0, 100}
	};

	const std::filesystem::path staging = leecherTree.Root() / "staging";

	REQUIRE(leecher.Begin("test/angel_maps", torrent->infoHash, staging, wanted, destinations, false)
		.has_value());

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

		if (leecher.Fetch().phase == FetchPhase::Failed) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	const wgrd::domain::FetchStatus status = leecher.Fetch();

	REQUIRE(status.phase == FetchPhase::Failed);
	REQUIRE(status.lastFailure == "fetch storage failed");

	const wgrd::domain::FetchStatus seederStatus = seeder.Fetch();

	REQUIRE(seederStatus.phase == FetchPhase::Idle);
	REQUIRE(seederStatus.hashFailures == 0);
	REQUIRE(seederStatus.bannedPeers == 0);
	REQUIRE(seederStatus.lastFailure.empty());
}

TEST_CASE("cancel clears destinations after removal confirms") {
	const TemporaryTree tree("cancel-deferred");

	TorrentSession session(tree.Root(), std::string(LOOPBACK), false);
	REQUIRE(AwaitPort(session) != 0);

	const std::filesystem::path targetFile = tree.Root() / "target" / "packs" / "ZZ_Win.dat";
	WriteFilled(targetFile, 100, char{0});

	const std::string chunkFileName = ChunkFileNaming::FileNameFor(DigestFor(0));

	const std::vector<wgrd::domain::ChunkDestination> destinations{
		wgrd::domain::ChunkDestination{chunkFileName, targetFile, 0, 100}
	};

	const auto begun = session.Begin(
		"test/angel_maps",
		DigestFor(7),
		tree.Root() / "staging",
		{chunkFileName},
		destinations,
		false
	);

	REQUIRE(begun.has_value());
	REQUIRE(session.RegisteredDestinations() == 1);

	session.Cancel();

	REQUIRE(session.Fetch().phase == FetchPhase::Idle);

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

	while (std::chrono::steady_clock::now() < deadline) {
		session.Poll();

		if (session.RegisteredDestinations() == 0) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	REQUIRE(session.RegisteredDestinations() == 0);
}
