#include "manager/tests/InstallServiceTestSupport.h"

#include "manager/announce/AnnounceCodec.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/install/InstalledReleaseStore.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/service/InstallService.h"
#include "manager/service/PublishService.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"

#include "domain/interfaces/content/IChunkFetcher.h"
#include "domain/interfaces/content/IChunkSetTorrentBuilder.h"
#include "domain/interfaces/content/IContentChunker.h"
#include "manager/install/ContentInstaller.h"
#include "domain/types/content/ChunkFileNaming.h"

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

using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkFileNaming;
using wgrd::manager::ContentInstaller;
using wgrd::domain::ChunkSpan;
using wgrd::domain::IChunkFetcher;
using wgrd::domain::IContentChunker;
using wgrd::domain::InstallPhase;
using wgrd::domain::ModManifest;
using wgrd::manager::Blake3Hasher;
using wgrd::manager::InstallService;
using wgrd::manager::ManifestBuilder;
using wgrd::manager::ManifestCodec;
using wgrd::manager::PayloadPathPolicy;
using wgrd::manager::PublishService;

using wgrd::manager::tests::AwaitPhase;
using wgrd::manager::tests::CHUNK_LENGTH;
using wgrd::manager::tests::CopyingFetcher;
using wgrd::manager::tests::FixedSizeChunker;
using wgrd::manager::tests::ReadAll;
using wgrd::manager::tests::StubTorrentBuilder;
using wgrd::manager::tests::TemporaryTree;
using wgrd::manager::tests::WritePayload;

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
	const StubTorrentBuilder torrentBuilder;
	const ManifestCodec codec(pathPolicy);
	const ManifestBuilder manifestBuilder(chunker, hasher, pathPolicy);

	wgrd::manager::DirectoryKeyRegistry registry(data.Root() / "registry");
	wgrd::manager::AnnounceReceiver receiver(registry, nullptr);

	PublishService publishService(
		publisherMods,
		data.Root(),
		chunker,
		hasher,
		pathPolicy,
		torrentBuilder,
		receiver,
		receiver,
		registry
	);

	REQUIRE(publishService.CreateKey("tester", data.Root() / "publisher.wgrdkey", "correct horse battery").has_value());

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

	const wgrd::manager::InstalledReleaseStore installedStore(data.Root() / "installed");

	InstallService installService(
		consumerMods,
		data.Root(),
		receiver,
		publishService.Store(),
		authenticator,
		codec,
		manifestBuilder,
		hasher,
		fetcher,
		installedStore,
		nullptr
	);

	REQUIRE(installService.CompletedInstalls() == 0);

	REQUIRE(installService.Start(published->identifier).has_value());

	REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

	REQUIRE(installService.CompletedInstalls() == 1);

	const wgrd::domain::InstallProgress progress = installService.Progress();

	REQUIRE(progress.remoteChunks == manifest->ChunkCount());
	REQUIRE(fetcher.Served() > 0);
	REQUIRE(progress.heldBytes == 0);

	const std::filesystem::path installed = consumerMods / "angel_maps";

	REQUIRE(ReadAll(installed / "packs" / "ZZ_Win.dat")
		== ReadAll(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat")
	);

	REQUIRE(ReadAll(installed / "mod.json")
		== ReadAll(publisherMods / "angel_maps" / "mod.json")
	);

	REQUIRE_FALSE(std::filesystem::exists(
			data.Root() / InstallService::STAGING_FOLDER / manifest->TorrentName())
	);

	std::size_t chunkFiles = 0;
	std::size_t leftoverStaging = 0;

	std::error_code walking;
	std::filesystem::recursive_directory_iterator walker(data.Root(), walking);
	const std::filesystem::recursive_directory_iterator end;

	for (; walker != end; walker.increment(walking)) {
		if (walking) {
			break;
		}

		if (walker->path().extension() == ChunkFileNaming::SUFFIX) {
			++chunkFiles;
		}
	}

	std::filesystem::recursive_directory_iterator installedWalker(consumerMods, walking);

	for (; installedWalker != end; installedWalker.increment(walking)) {
		if (walking) {
			break;
		}

		const std::string leaf = installedWalker->path().filename().string();

		if (leaf.ends_with(ContentInstaller::STAGING_SUFFIX)) {
			++leftoverStaging;
		}

		if (installedWalker->path().extension() == ChunkFileNaming::SUFFIX) {
			++chunkFiles;
		}
	}

	REQUIRE(chunkFiles == 0);
	REQUIRE(leftoverStaging == 0);

	REQUIRE(installService.Verify(published->identifier).has_value());
	REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

	{
		std::fstream damage(
			installed / "packs" / "ZZ_Win.dat",
			std::ios::binary | std::ios::in | std::ios::out
		);

		REQUIRE(damage.good());

		damage.seekp(64);
		damage.put('\x7F');
	}

	const std::size_t servedBeforeRepair = fetcher.Served();

	REQUIRE(installService.Verify(published->identifier).has_value());
	REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

	REQUIRE(fetcher.Served() > servedBeforeRepair);

	REQUIRE(ReadAll(installed / "packs" / "ZZ_Win.dat")
		== ReadAll(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat")
	);
}

TEST_CASE("reinstalling an unchanged mod fetches nothing") {
	const TemporaryTree publisher("publisher-reuse");
	const TemporaryTree data("data-reuse");

	const std::filesystem::path publisherMods = publisher.Root() / "Mods";

	WritePayload(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat", 9000, 5);

	const FixedSizeChunker chunker;
	const Blake3Hasher hasher;
	const PayloadPathPolicy pathPolicy;
	const StubTorrentBuilder torrentBuilder;
	const ManifestCodec codec(pathPolicy);
	const ManifestBuilder manifestBuilder(chunker, hasher, pathPolicy);

	wgrd::manager::DirectoryKeyRegistry registry(data.Root() / "registry");
	wgrd::manager::AnnounceReceiver receiver(registry, nullptr);

	PublishService publishService(
		publisherMods,
		data.Root(),
		chunker,
		hasher,
		pathPolicy,
		torrentBuilder,
		receiver,
		receiver,
		registry
	);

	REQUIRE(publishService.CreateKey("tester", data.Root() / "publisher.wgrdkey", "correct horse battery").has_value());
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

	const wgrd::manager::InstalledReleaseStore installedStore(data.Root() / "installed");

	InstallService installService(
		publisherMods,
		data.Root(),
		receiver,
		publishService.Store(),
		authenticator,
		codec,
		manifestBuilder,
		hasher,
		fetcher,
		installedStore,
		nullptr
	);

	const auto payloadStampBefore = std::filesystem::last_write_time(
		publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat"
	);

	REQUIRE(installService.Start(published->identifier).has_value());
	REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

	REQUIRE(installService.Progress().remoteChunks == 0);
	REQUIRE(fetcher.Served() == 0);
	REQUIRE(installService.Progress().heldBytes == 0);

	REQUIRE(std::filesystem::last_write_time(
			publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat"
		) == payloadStampBefore
	);

	std::size_t stagedTwins = 0;

	std::error_code walking;
	std::filesystem::recursive_directory_iterator walker(publisherMods, walking);
	const std::filesystem::recursive_directory_iterator walkEnd;

	for (; walker != walkEnd; walker.increment(walking)) {
		if (walking) {
			break;
		}

		if (walker->path().filename().string().ends_with(ContentInstaller::STAGING_SUFFIX)) {
			++stagedTwins;
		}
	}

	REQUIRE(stagedTwins == 0);
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
	const StubTorrentBuilder torrentBuilder;
	const ManifestCodec codec(pathPolicy);
	const ManifestBuilder manifestBuilder(chunker, hasher, pathPolicy);

	wgrd::manager::DirectoryKeyRegistry registry(publisher.Root() / "registry");
	wgrd::manager::AnnounceReceiver publisherReceiver(registry, nullptr);

	PublishService publishService(
		publisherMods,
		publisher.Root(),
		chunker,
		hasher,
		pathPolicy,
		torrentBuilder,
		publisherReceiver,
		publisherReceiver,
		registry
	);

	REQUIRE(publishService.CreateKey(
			"tester",
			publisher.Root() / "publisher.wgrdkey",
			"correct horse battery").has_value()
	);
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

	wgrd::manager::AnnounceReceiver consumerReceiver(registry, nullptr);
	REQUIRE(consumerReceiver.Accept(
			wgrd::manager::AnnounceCodec::Encode(*announce)).has_value()
	);

	const wgrd::manager::ManifestStore consumerStore(consumer.Root() / "manifests");
	REQUIRE_FALSE(consumerStore.Holds(announce->manifestDigest.ToHex()));

	CopyingFetcher fetcher(*manifest, publisherMods / "angel_maps", sealedPath);

	const wgrd::manager::InstalledReleaseStore installedStore(consumer.Root() / "installed");

	InstallService installService(
		consumerMods,
		consumer.Root(),
		consumerReceiver,
		consumerStore,
		authenticator,
		codec,
		manifestBuilder,
		hasher,
		fetcher,
		installedStore,
		nullptr
	);

	REQUIRE(installService.Start(published->identifier).has_value());
	REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

	REQUIRE(consumerStore.Holds(announce->manifestDigest.ToHex()));

	REQUIRE(ReadAll(consumerMods / "angel_maps" / "packs" / "ZZ_Win.dat")
		== ReadAll(publisherMods / "angel_maps" / "packs" / "ZZ_Win.dat")
	);
}
