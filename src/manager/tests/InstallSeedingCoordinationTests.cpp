#include "manager/announce/AnnounceReceiver.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/install/InstalledReleaseStore.h"
#include "manager/manifest/ManifestBuilder.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/service/InstallService.h"
#include "manager/service/PublishService.h"
#include "manager/text/ServiceText.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"

#include "domain/interfaces/content/IChunkFetcher.h"
#include "domain/interfaces/content/IChunkSetTorrentBuilder.h"
#include "domain/interfaces/content/IContentChunker.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/types/content/ChunkFileNaming.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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
using wgrd::domain::FetchError;
using wgrd::domain::IChunkFetcher;
using wgrd::domain::IContentChunker;
using wgrd::domain::InstallPhase;
using wgrd::domain::ISeedingService;
using wgrd::domain::ModManifest;
using wgrd::manager::InstallService;

namespace {
constexpr std::size_t CHUNK_LENGTH = 2048;
constexpr std::uint64_t PAYLOAD_BYTES = 16 * 1024;
constexpr std::string_view MOD_FOLDER = "testmod";

class FixedSizeChunker final : public IContentChunker {
public:
	~FixedSizeChunker() override = default;

	[[nodiscard]] std::vector<wgrd::domain::ChunkSpan> Split(
		const std::span<const std::byte> data
	) const override {
		std::vector<wgrd::domain::ChunkSpan> spans;

		std::size_t offset = 0;
		while (offset < data.size()) {
			const std::size_t length = std::min(CHUNK_LENGTH, data.size() - offset);
			spans.push_back(wgrd::domain::ChunkSpan{offset, length});
			offset += length;
		}

		return spans;
	}
};

class StubTorrentBuilder final : public wgrd::domain::IChunkSetTorrentBuilder {
public:
	~StubTorrentBuilder() override = default;

	[[nodiscard]] std::expected<wgrd::domain::ChunkSetTorrentDescription, wgrd::domain::ChunkSetTorrentError> Build(
		const ModManifest& manifest,
		const std::filesystem::path&,
		std::span<const std::uint8_t>
	) const override {
		const auto infoHash = ChunkDigest::FromHex(std::string(64, 'a'));
		REQUIRE(infoHash.has_value());

		return wgrd::domain::ChunkSetTorrentDescription{
			{'d', 'e'}, *infoHash, manifest.TotalBytes(), manifest.ChunkCount()
		};
	}
};

class RecordingSeedingService final : public ISeedingService {
public:
	RecordingSeedingService() = default;

	~RecordingSeedingService() override = default;

	[[nodiscard]] bool Enabled() const override {
		return true;
	}

	void SetEnabled(bool) override {
	}

	[[nodiscard]] std::expected<wgrd::domain::SeedEntry, wgrd::domain::SeedError> Announce(
		const ModManifest& manifest,
		const std::filesystem::path&,
		const std::filesystem::path&
	) override {
		++announceCalls;

		const wgrd::domain::SeedEntry entry{
			manifest.Identifier(),
			manifest.ModName(),
			manifest.Version(),
			manifest.TotalBytes(),
			manifest.ChunkCount(),
			std::string(64, 'a'),
			true,
			0,
			0
		};

		_entries.assign(1, entry);

		return entry;
	}

	void AttestContent(
		const wgrd::domain::ModManifest&,
		const std::filesystem::path&,
		const std::filesystem::path&
	) override {
		++attestCalls;
	}

	bool StopSeeding(std::string_view) override {
		++stopCalls;

		const bool held = !_entries.empty();
		_entries.clear();

		return held;
	}

	[[nodiscard]] const std::vector<wgrd::domain::SeedEntry>& Entries() const override {
		return _entries;
	}

	[[nodiscard]] std::uint64_t UploadedBytes() const override {
		return 0;
	}

	std::size_t announceCalls = 0;
	std::size_t stopCalls = 0;
	std::size_t attestCalls = 0;

private:
	std::vector<wgrd::domain::SeedEntry> _entries;
};

class SeedAwareFetcher final : public IChunkFetcher {
public:
	SeedAwareFetcher(
		const ModManifest& manifest,
		std::filesystem::path sourceFolder,
		const ISeedingService& seeding
	)
		: _manifest(manifest)
		, _sourceFolder(std::move(sourceFolder))
		, _seeding(&seeding)
		, _status() {}

	~SeedAwareFetcher() override = default;

	[[nodiscard]] std::expected<void, FetchError> Begin(
		std::string identifier,
		const ChunkDigest&,
		const std::filesystem::path& stagingFolder,
		const std::vector<std::string>&,
		const std::vector<wgrd::domain::ChunkDestination>& destinations,
		bool
	) override {
		seedEntriesAtBegin.push_back(_seeding->Entries().size());

		for (const wgrd::domain::ChunkDestination& destination : destinations) {
			for (const auto& file : _manifest.Files()) {
				for (const auto& chunk : file.chunks) {
					if (ChunkFileNaming::FileNameFor(chunk.digest) != destination.chunkFileName) {
						continue;
					}

					std::ifstream input(_sourceFolder / file.path, std::ios::binary);
					input.seekg(static_cast<std::streamoff>(chunk.offset));

					std::vector<char> bytes(chunk.length);
					input.read(bytes.data(), chunk.length);

					std::fstream output(
						destination.file,
						std::ios::binary | std::ios::in | std::ios::out
					);

					output.seekp(static_cast<std::streamoff>(destination.offset));
					output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
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

	std::vector<std::size_t> seedEntriesAtBegin;

private:
	ModManifest _manifest;
	std::filesystem::path _sourceFolder;
	const ISeedingService* _seeding;
	wgrd::domain::FetchStatus _status;
};

class RefusingFetcher final : public IChunkFetcher {
public:
	RefusingFetcher() = default;

	~RefusingFetcher() override = default;

	[[nodiscard]] std::expected<void, FetchError> Begin(
		std::string,
		const ChunkDigest&,
		const std::filesystem::path&,
		const std::vector<std::string>&,
		const std::vector<wgrd::domain::ChunkDestination>&,
		bool
	) override {
		++refusals;
		return std::unexpected(FetchError::AlreadyPresent);
	}

	[[nodiscard]] wgrd::domain::FetchStatus Fetch() const override {
		return {};
	}

	void Cancel() override {
	}

	std::size_t refusals = 0;
};

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-install-seed" / label;

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

void WritePayload(const std::filesystem::path& target, const std::uint64_t bytes) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::uint64_t position = 0; position < bytes; ++position) {
		output.put(static_cast<char>((position * 37 + 11) & 0xFF));
	}
}

bool AwaitPhase(InstallService& service, const InstallPhase phase, const std::chrono::milliseconds timeout) {
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

class PublishedWorld {
public:
	explicit PublishedWorld(const TemporaryTree& tree)
		: publisherMods(tree.Root() / "PublisherMods")
		, consumerMods(tree.Root() / "Mods")
		, dataDirectory(tree.Root() / "data")
		, registry(dataDirectory / "registry")
		, receiver(registry, nullptr)
		, authenticator(registry)
		, codec(pathPolicy)
		, manifestBuilder(chunker, hasher, pathPolicy)
		, installedStore(dataDirectory / "installed")
		, publishService(
			  publisherMods,
			  dataDirectory,
			  chunker,
			  hasher,
			  pathPolicy,
			  torrentBuilder,
			  receiver,
			  receiver,
			  registry
		  ) {
		WritePayload(publisherMods / MOD_FOLDER / "pack.dat", PAYLOAD_BYTES);

		{
			std::ofstream metadata(
				publisherMods / MOD_FOLDER / "mod.json",
				std::ios::binary | std::ios::trunc
			);
			metadata << R"({"name":"testmod","version":"1.0.0"})";
		}

		REQUIRE(publishService.CreateKey(
				"tester",
				tree.Root() / "publisher.wgrdkey",
				"correct horse battery").has_value()
		);

		registry.Reload();

		const auto published = publishService.Publish(std::string(MOD_FOLDER));
		REQUIRE(published.has_value());
		identifier = published->identifier;

		const auto announce = receiver.Retained(identifier);
		REQUIRE(announce.has_value());

		const auto sealed = publishService.Store().Load(announce->manifestDigest.ToHex());
		REQUIRE(sealed.has_value());

		const auto authenticated = authenticator.Authenticate(*sealed);
		REQUIRE(authenticated.has_value());

		const auto decoded = codec.Decode(authenticated->payload);
		REQUIRE(decoded.has_value());
		manifest = *decoded;

		sealedManifestPath = publishService.Store().PathFor(announce->manifestDigest.ToHex());
	}

	std::filesystem::path publisherMods;
	std::filesystem::path consumerMods;
	std::filesystem::path dataDirectory;
	FixedSizeChunker chunker;
	wgrd::manager::Blake3Hasher hasher;
	wgrd::manager::PayloadPathPolicy pathPolicy;
	StubTorrentBuilder torrentBuilder;
	wgrd::manager::DirectoryKeyRegistry registry;
	wgrd::manager::AnnounceReceiver receiver;
	wgrd::manager::ManifestAuthenticator authenticator;
	wgrd::manager::ManifestCodec codec;
	wgrd::manager::ManifestBuilder manifestBuilder;
	wgrd::manager::InstalledReleaseStore installedStore;
	wgrd::manager::PublishService publishService;
	std::string identifier;
	ModManifest manifest;
	std::filesystem::path sealedManifestPath;
};

void DamageInstalledPayload(const std::filesystem::path& target) {
	std::fstream damage(target, std::ios::binary | std::ios::in | std::ios::out);
	REQUIRE(damage.good());

	damage.seekp(64);
	damage.put('\x7F');
}
}

TEST_CASE("repair withdraws the seed before fetching") {
	const TemporaryTree tree("withdraw");
	PublishedWorld world(tree);

	RecordingSeedingService seeding;
	SeedAwareFetcher fetcher(world.manifest, world.publisherMods / MOD_FOLDER, seeding);

	InstallService installService(
		world.consumerMods,
		world.dataDirectory,
		world.receiver,
		world.publishService.Store(),
		world.authenticator,
		world.codec,
		world.manifestBuilder,
		world.hasher,
		fetcher,
		world.installedStore,
		&seeding
	);

	REQUIRE(installService.Start(world.identifier).has_value());
	REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));
	REQUIRE(installService.SettledAttempts() == 1);

	REQUIRE(seeding.Announce(
			world.manifest,
			world.consumerMods / MOD_FOLDER,
			world.sealedManifestPath
		).has_value()
	);
	REQUIRE(seeding.Entries().size() == 1);

	const std::size_t stopsBeforeIntactVerify = seeding.stopCalls;

	REQUIRE(installService.Verify(world.identifier).has_value());
	REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

	REQUIRE(seeding.stopCalls == stopsBeforeIntactVerify);
	REQUIRE(seeding.Entries().size() == 1);
	REQUIRE(installService.SettledAttempts() == 2);

	DamageInstalledPayload(world.consumerMods / MOD_FOLDER / "pack.dat");

	const std::size_t stopsBeforeRepair = seeding.stopCalls;
	const std::size_t beginsBeforeRepair = fetcher.seedEntriesAtBegin.size();

	REQUIRE(installService.Verify(world.identifier).has_value());
	REQUIRE(AwaitPhase(installService, InstallPhase::Done, std::chrono::seconds(20)));

	REQUIRE(seeding.stopCalls == stopsBeforeRepair + 1);
	REQUIRE(fetcher.seedEntriesAtBegin.size() == beginsBeforeRepair + 1);
	REQUIRE(fetcher.seedEntriesAtBegin.back() == 0);
	REQUIRE(installService.SettledAttempts() == 3);
}

TEST_CASE("a refused duplicate fetch settles as seed conflict") {
	const TemporaryTree tree("conflict");
	PublishedWorld world(tree);

	RecordingSeedingService seeding;
	SeedAwareFetcher installFetcher(world.manifest, world.publisherMods / MOD_FOLDER, seeding);

	InstallService installFirst(
		world.consumerMods,
		world.dataDirectory,
		world.receiver,
		world.publishService.Store(),
		world.authenticator,
		world.codec,
		world.manifestBuilder,
		world.hasher,
		installFetcher,
		world.installedStore,
		&seeding
	);

	REQUIRE(installFirst.Start(world.identifier).has_value());
	REQUIRE(AwaitPhase(installFirst, InstallPhase::Done, std::chrono::seconds(20)));

	DamageInstalledPayload(world.consumerMods / MOD_FOLDER / "pack.dat");

	RefusingFetcher refusingFetcher;

	InstallService repairService(
		world.consumerMods,
		world.dataDirectory,
		world.receiver,
		world.publishService.Store(),
		world.authenticator,
		world.codec,
		world.manifestBuilder,
		world.hasher,
		refusingFetcher,
		world.installedStore,
		&seeding
	);

	REQUIRE(repairService.Verify(world.identifier).has_value());
	REQUIRE(AwaitPhase(repairService, InstallPhase::Failed, std::chrono::seconds(20)));

	REQUIRE(refusingFetcher.refusals == 1);
	REQUIRE(repairService.Progress().message == wgrd::manager::text::INSTALL_SEED_CONFLICT);
	REQUIRE(repairService.SettledAttempts() == 1);
}
