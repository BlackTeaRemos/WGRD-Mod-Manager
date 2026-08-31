#include "downloader/storage/ModFolderStamp.h"
#include "downloader/storage/SeedAttestations.h"
#include "downloader/storage/SeedStampStore.h"

#include "domain/types/content/ModManifest.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::domain::PublisherFingerprint;
using wgrd::downloader::ModFolderStamp;
using wgrd::downloader::SeedAttestations;
using wgrd::downloader::SeedStampStore;

namespace {
constexpr std::string_view KEY = "51915f87e6293ae4";

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-attest" / label;

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

ModManifest BuildManifest() {
	const auto digest = ChunkDigest::FromHex(std::format("{:064x}", 1));
	REQUIRE(digest.has_value());

	std::vector<ManifestChunk> chunks{ManifestChunk{*digest, 0, 64}};
	ManifestFile file{"packs/ZZ_Win.dat", 64, std::move(chunks)};

	return ModManifest(PublisherFingerprint{}, "angel_maps", 1, {file});
}

void WriteFile(const std::filesystem::path& target, const char fill) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (int index = 0; index < 64; ++index) {
		output.put(fill);
	}
}
}

TEST_CASE("attestations track torrent names") {
	SeedAttestations attestations;

	REQUIRE_FALSE(attestations.Attests("fingerprint_angel_maps"));

	attestations.Mark("fingerprint_angel_maps");

	REQUIRE(attestations.Attests("fingerprint_angel_maps"));
	REQUIRE(attestations.Count() == 1);

	attestations.Mark("fingerprint_angel_maps");
	REQUIRE(attestations.Count() == 1);

	attestations.Forget("fingerprint_angel_maps");

	REQUIRE_FALSE(attestations.Attests("fingerprint_angel_maps"));
	REQUIRE(attestations.Count() == 0);
}

TEST_CASE("stamp changes when a mod file changes") {
	const TemporaryTree tree("stamp");

	const ModManifest manifest = BuildManifest();
	const std::filesystem::path modFolder = tree.Root() / "mod";
	const std::filesystem::path payload = modFolder / "packs" / "ZZ_Win.dat";

	WriteFile(payload, 0x11);

	const std::string first = ModFolderStamp::Compute(manifest, modFolder);

	REQUIRE_FALSE(first.empty());
	REQUIRE(first == ModFolderStamp::Compute(manifest, modFolder));

	const std::filesystem::file_time_type written = std::filesystem::last_write_time(payload);
	std::filesystem::last_write_time(payload, written + std::chrono::seconds(5));

	REQUIRE(ModFolderStamp::Compute(manifest, modFolder) != first);

	std::filesystem::remove(payload);

	const std::string missing = ModFolderStamp::Compute(manifest, modFolder);

	REQUIRE(missing != first);
	REQUIRE(missing.find(ModFolderStamp::MISSING_MARK) != std::string::npos);
}

TEST_CASE("stamp store round trips and rejects bad keys") {
	const TemporaryTree tree("stamp-store");

	const SeedStampStore store(tree.Root() / "torrents");

	REQUIRE_FALSE(store.Load(KEY).has_value());

	REQUIRE(store.Save(KEY, "packs/ZZ_Win.dat 64 12345\n"));

	const auto loaded = store.Load(KEY);

	REQUIRE(loaded.has_value());
	REQUIRE(*loaded == "packs/ZZ_Win.dat 64 12345\n");

	REQUIRE_FALSE(store.Save("not hex", "x"));
	REQUIRE_FALSE(store.Load("not hex").has_value());
	REQUIRE_FALSE(store.Save(KEY, std::string()));

	store.Forget(KEY);

	REQUIRE_FALSE(store.Load(KEY).has_value());
}
