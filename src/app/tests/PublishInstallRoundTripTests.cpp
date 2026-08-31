#include "downloader/chunk/FastCdcChunker.h"
#include "downloader/torrent/build/ChunkSetTorrentBuilder.h"
#include "downloader/transfer/TorrentSession.h"

#include "manager/announce/AnnounceCodec.h"
#include "manager/announce/AnnounceReceiver.h"
#include "manager/announce/AnnounceStore.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/install/InstalledReleaseStore.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/service/CatalogService.h"
#include "manager/service/InstallService.h"
#include "manager/service/PublishService.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"

#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {
constexpr std::string_view LOOPBACK = "127.0.0.1:0";
constexpr std::string_view PUBLISHER = "tester";
constexpr std::string_view PASSPHRASE = "correcthorse";
constexpr std::string_view MOD_FOLDER = "roundtripmod";
constexpr std::uint64_t PACK_BYTES = 6 * 1024 * 1024;

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-roundtrip" / label;

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

void WritePattern(
	const std::filesystem::path& target,
	const std::uint64_t bytes,
	const std::uint64_t seed
) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);

	for (std::uint64_t position = 0; position < bytes; ++position) {
		const std::uint64_t block = position / 65536;
		const std::uint64_t value = block % 3 == 1
		                            ? 0
		                            : position * 131 + seed * 17 + block;

		output.put(static_cast<char>(value & 0xFF));
	}
}

std::vector<char> ReadWhole(const std::filesystem::path& source) {
	std::ifstream input(source, std::ios::binary);

	const std::istreambuf_iterator<char> begin(input);
	const std::istreambuf_iterator<char> end;

	return std::vector<char>(begin, end);
}
}

TEST_CASE("published mod installs byte identical on a second peer") {
	const TemporaryTree publisherTree("publisher");
	const TemporaryTree leecherTree("leecher");

	const std::filesystem::path publisherMods = publisherTree.Root() / "Mods";
	const std::filesystem::path publisherData = publisherMods / ".wgrdmm";

	std::error_code failure;
	std::filesystem::create_directories(publisherMods, failure);

	WritePattern(publisherMods / MOD_FOLDER / "packs" / "ZZ_Win.dat", PACK_BYTES, 1);
	WritePattern(publisherMods / MOD_FOLDER / "packs" / "ZZ_Data.dat", PACK_BYTES / 2, 2);

	{
		std::ofstream metadata(
			publisherMods / MOD_FOLDER / "mod.json",
			std::ios::binary | std::ios::trunc
		);
		metadata << R"({"name":"roundtripmod","version":"1.0.0"})";
	}

	const wgrd::downloader::FastCdcChunker chunker(wgrd::domain::ChunkSizes::Default());
	const wgrd::manager::Blake3Hasher hasher;
	const wgrd::manager::PayloadPathPolicy pathPolicy;
	const wgrd::manager::ManifestCodec codec(pathPolicy);
	const wgrd::downloader::ChunkSetTorrentBuilder torrentBuilder;
	const wgrd::manager::ManifestBuilder manifestBuilder(chunker, hasher, pathPolicy);

	wgrd::manager::DirectoryKeyRegistry publisherRegistry(publisherData / "registry");
	wgrd::manager::AnnounceStore publisherAnnounces(publisherData / "announces");
	wgrd::manager::AnnounceReceiver publisherReceiver(publisherRegistry, &publisherAnnounces);
	const wgrd::manager::ManifestAuthenticator publisherAuthenticator(publisherRegistry);

	wgrd::downloader::TorrentSession publisherSwarm(
		publisherMods,
		std::string(LOOPBACK),
		false
	);

	wgrd::manager::PublishService publishService(
		publisherMods,
		publisherData,
		chunker,
		hasher,
		pathPolicy,
		torrentBuilder,
		publisherReceiver,
		publisherReceiver,
		publisherRegistry,
		&publisherSwarm
	);

	const wgrd::manager::InstalledReleaseStore publisherInstalled(publisherData / "installed");

	wgrd::manager::CatalogService publisherCatalog(
		publisherMods,
		publisherReceiver,
		publishService.Store(),
		publisherAuthenticator,
		codec,
		publisherRegistry,
		publisherInstalled,
		&publisherSwarm
	);

	REQUIRE(publishService.CreateKey(
			PUBLISHER,
			publisherTree.Root() / "publisher.wgrdkey",
			PASSPHRASE
		).has_value()
	);

	publishService.RefreshCandidates();

	const auto published = publishService.Publish(MOD_FOLDER);

	INFO("publish message " << publishService.LastMessage());
	REQUIRE(published.has_value());

	publisherCatalog.Refresh();

	REQUIRE(publisherCatalog.Rows().size() == 1);
	REQUIRE(publisherCatalog.Rows()[0].seedFault.empty());
	REQUIRE(publisherSwarm.Entries().size() == 1);

	const std::filesystem::path leecherMods = leecherTree.Root() / "Mods";
	const std::filesystem::path leecherData = leecherMods / ".wgrdmm";

	std::filesystem::create_directories(leecherData / "registry", failure);
	std::filesystem::copy(
		publisherData / "registry",
		leecherData / "registry",
		std::filesystem::copy_options::recursive,
		failure
	);

	REQUIRE_FALSE(failure);

	wgrd::manager::DirectoryKeyRegistry leecherRegistry(leecherData / "registry");
	wgrd::manager::AnnounceStore leecherAnnounces(leecherData / "announces");
	wgrd::manager::AnnounceReceiver leecherReceiver(leecherRegistry, &leecherAnnounces);
	const wgrd::manager::ManifestAuthenticator leecherAuthenticator(leecherRegistry);

	const auto announce = publisherReceiver.Retained(published->identifier);
	REQUIRE(announce.has_value());

	REQUIRE(leecherReceiver.Accept(wgrd::manager::AnnounceCodec::Encode(*announce)).has_value());

	wgrd::downloader::TorrentSession leecherSwarm(
		leecherMods,
		std::string(LOOPBACK),
		false
	);

	const wgrd::manager::ManifestStore leecherStore(leecherData / "manifests");
	const wgrd::manager::InstalledReleaseStore leecherInstalled(leecherData / "installed");

	wgrd::manager::InstallService installService(
		leecherMods,
		leecherData,
		leecherReceiver,
		leecherStore,
		leecherAuthenticator,
		codec,
		manifestBuilder,
		hasher,
		leecherSwarm,
		leecherInstalled,
		&leecherSwarm
	);

	REQUIRE(installService.Start(published->identifier).has_value());

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
	auto nextDial = std::chrono::steady_clock::now();

	while (std::chrono::steady_clock::now() < deadline) {
		publisherSwarm.Poll();
		leecherSwarm.Poll();
		installService.Poll();

		if (std::chrono::steady_clock::now() >= nextDial) {
			leecherSwarm.ConnectLocalPeer(publisherSwarm.ListenPort());
			nextDial = std::chrono::steady_clock::now() + std::chrono::seconds(1);
		}

		const wgrd::domain::InstallProgress progress = installService.Progress();

		if (progress.phase == wgrd::domain::InstallPhase::Done
		    || progress.phase == wgrd::domain::InstallPhase::Failed) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	const wgrd::domain::InstallProgress progress = installService.Progress();

	INFO("install message " << progress.message);
	INFO("hash failures " << progress.hashFailures << " banned " << progress.bannedPeers);
	REQUIRE(progress.phase == wgrd::domain::InstallPhase::Done);

	for (const std::string& relative : {
		     std::string("packs/ZZ_Win.dat"), std::string("packs/ZZ_Data.dat"), std::string("mod.json")
	     }) {
		const std::vector<char> original = ReadWhole(publisherMods / MOD_FOLDER / relative);
		const std::vector<char> received = ReadWhole(leecherMods / MOD_FOLDER / relative);

		INFO("comparing " << relative);
		REQUIRE(received.size() == original.size());

		std::size_t firstMismatch = original.size();
		std::size_t mismatchCount = 0;
		std::size_t zeroMismatches = 0;
		std::size_t runCount = 0;
		bool inRun = false;

		for (std::size_t index = 0; index < original.size(); ++index) {
			const bool differs = received[index] != original[index];

			if (differs) {
				if (mismatchCount == 0) {
					firstMismatch = index;
				}

				if (received[index] == 0) {
					++zeroMismatches;
				}

				++mismatchCount;

				if (!inRun) {
					++runCount;
					inRun = true;
				}
			} else {
				inRun = false;
			}
		}

		INFO("first mismatch " << firstMismatch << " of " << original.size());
		INFO("mismatched bytes " << mismatchCount);
		INFO("mismatches that are zero " << zeroMismatches);
		INFO("contiguous mismatch runs " << runCount);
		REQUIRE(mismatchCount == 0);
	}
}
