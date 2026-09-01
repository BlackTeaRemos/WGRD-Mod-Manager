#include "manager/announce/AnnounceReceiver.h"
#include "manager/hash/Blake3Hasher.h"
#include "manager/install/InstalledReleaseStore.h"
#include "manager/manifest/ManifestCodec.h"
#include "manager/payload/PayloadPathPolicy.h"
#include "manager/service/CatalogService.h"
#include "manager/service/PublishService.h"
#include "manager/text/ServiceText.h"
#include "manager/trust/DirectoryKeyRegistry.h"
#include "manager/trust/ManifestAuthenticator.h"

#include "domain/interfaces/content/IChunkSetTorrentBuilder.h"
#include "domain/interfaces/content/IContentChunker.h"
#include "domain/interfaces/services/ISeedingService.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace {
constexpr std::size_t CHUNK_LENGTH = 2048;
constexpr std::uint64_t PAYLOAD_BYTES = 16 * 1024;
constexpr std::string_view MOD_FOLDER = "testmod";

class FixedSizeChunker final : public wgrd::domain::IContentChunker {
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
		const wgrd::domain::ModManifest& manifest,
		const std::filesystem::path&,
		std::span<const std::uint8_t>
	) const override {
		const auto infoHash = wgrd::domain::ChunkDigest::FromHex(std::string(64, 'a'));
		REQUIRE(infoHash.has_value());

		return wgrd::domain::ChunkSetTorrentDescription{
			{'d', 'e'}, *infoHash, manifest.TotalBytes(), manifest.ChunkCount()
		};
	}
};

class RecordingSeedingService final : public wgrd::domain::ISeedingService {
public:
	explicit RecordingSeedingService(std::string reportedInfoHash)
		: _reportedInfoHash(std::move(reportedInfoHash)) {}

	~RecordingSeedingService() override = default;

	[[nodiscard]] bool Enabled() const override {
		return true;
	}

	void SetEnabled(bool) override {
	}

	[[nodiscard]] std::expected<wgrd::domain::SeedEntry, wgrd::domain::SeedError> Announce(
		const wgrd::domain::ModManifest& manifest,
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
			_reportedInfoHash,
			true,
			0,
			0
		};

		_entries.assign(1, entry);

		return entry;
	}

	void PrepareSeed(
		const wgrd::domain::ModManifest&,
		const std::filesystem::path&,
		const std::filesystem::path&
	) override {
		++prepareCalls;
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
		_entries.clear();
		return true;
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
	std::size_t prepareCalls = 0;

private:
	std::string _reportedInfoHash;
	std::vector<wgrd::domain::SeedEntry> _entries;
};

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-catalog-seed" / label;

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

class PublishedWorld {
public:
	explicit PublishedWorld(const TemporaryTree& tree)
		: modsDirectory(tree.Root() / "Mods")
		, dataDirectory(tree.Root() / "data")
		, registry(dataDirectory / "registry")
		, receiver(registry, nullptr)
		, authenticator(registry)
		, codec(pathPolicy)
		, installedStore(dataDirectory / "installed")
		, publishService(
			  modsDirectory,
			  dataDirectory,
			  chunker,
			  hasher,
			  pathPolicy,
			  torrentBuilder,
			  receiver,
			  receiver,
			  registry
		  ) {
		WritePayload(modsDirectory / MOD_FOLDER / "pack.dat", PAYLOAD_BYTES);

		{
			std::ofstream metadata(
				modsDirectory / MOD_FOLDER / "mod.json",
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
	}

	std::filesystem::path modsDirectory;
	std::filesystem::path dataDirectory;
	FixedSizeChunker chunker;
	wgrd::manager::Blake3Hasher hasher;
	wgrd::manager::PayloadPathPolicy pathPolicy;
	StubTorrentBuilder torrentBuilder;
	wgrd::manager::DirectoryKeyRegistry registry;
	wgrd::manager::AnnounceReceiver receiver;
	wgrd::manager::ManifestAuthenticator authenticator;
	wgrd::manager::ManifestCodec codec;
	wgrd::manager::InstalledReleaseStore installedStore;
	wgrd::manager::PublishService publishService;
};
}

TEST_CASE("an outdated install is not seeded") {
	const TemporaryTree tree("outdated");
	PublishedWorld world(tree);

	const auto firstRelease = world.publishService.Publish(MOD_FOLDER);
	REQUIRE(firstRelease.has_value());

	const auto secondRelease = world.publishService.Publish(MOD_FOLDER);
	REQUIRE(secondRelease.has_value());
	REQUIRE(secondRelease->version > firstRelease->version);

	REQUIRE(world.installedStore.Save(
			wgrd::domain::InstalledRelease{
				firstRelease->identifier,
				firstRelease->modName,
				firstRelease->version,
				firstRelease->manifestDigest
			}
		)
	);

	RecordingSeedingService seeding(std::string(64, 'a'));

	wgrd::manager::CatalogService catalog(
		world.modsDirectory,
		world.receiver,
		world.publishService.Store(),
		world.authenticator,
		world.codec,
		world.registry,
		world.installedStore,
		&seeding
	);

	REQUIRE(catalog.Rows().size() == 1);
	REQUIRE(catalog.Rows()[0].Outdated());
	REQUIRE(seeding.announceCalls == 0);
	REQUIRE(seeding.stopCalls > 0);
	REQUIRE(seeding.Entries().empty());
}

TEST_CASE("a version matched install is seeded") {
	const TemporaryTree tree("matched");
	PublishedWorld world(tree);

	const auto release = world.publishService.Publish(MOD_FOLDER);
	REQUIRE(release.has_value());

	REQUIRE(world.installedStore.Save(
			wgrd::domain::InstalledRelease{
				release->identifier,
				release->modName,
				release->version,
				release->manifestDigest
			}
		)
	);

	RecordingSeedingService seeding(std::string(64, 'a'));

	wgrd::manager::CatalogService catalog(
		world.modsDirectory,
		world.receiver,
		world.publishService.Store(),
		world.authenticator,
		world.codec,
		world.registry,
		world.installedStore,
		&seeding
	);

	REQUIRE(catalog.Rows().size() == 1);
	REQUIRE(seeding.announceCalls == 1);
	REQUIRE(seeding.stopCalls == 0);
	REQUIRE(catalog.Rows()[0].seedFault.empty());
}

TEST_CASE("an infohash mismatch withdraws the seed") {
	const TemporaryTree tree("mismatch");
	PublishedWorld world(tree);

	const auto release = world.publishService.Publish(MOD_FOLDER);
	REQUIRE(release.has_value());

	REQUIRE(world.installedStore.Save(
			wgrd::domain::InstalledRelease{
				release->identifier,
				release->modName,
				release->version,
				release->manifestDigest
			}
		)
	);

	RecordingSeedingService seeding(std::string(64, 'b'));

	wgrd::manager::CatalogService catalog(
		world.modsDirectory,
		world.receiver,
		world.publishService.Store(),
		world.authenticator,
		world.codec,
		world.registry,
		world.installedStore,
		&seeding
	);

	REQUIRE(catalog.Rows().size() == 1);
	REQUIRE(seeding.announceCalls == 1);
	REQUIRE(seeding.stopCalls == 1);
	REQUIRE(seeding.Entries().empty());
	REQUIRE(catalog.Rows()[0].seedFault == wgrd::manager::text::SEED_INFOHASH_MISMATCH);
}
