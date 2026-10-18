#include "manager/install/EmptyChunkCatalogue.h"
#include "manager/install/ManifestDiffer.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkSourceKind;
using wgrd::domain::HeldChunkLocation;
using wgrd::domain::IHeldChunkCatalogue;
using wgrd::domain::InstallPlan;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::manager::EmptyChunkCatalogue;
using wgrd::manager::ManifestDiffer;

namespace {
const std::filesystem::path HELD_ROOT = std::filesystem::path("C:/mods/mod");
const std::filesystem::path BASELINE_ROOT = std::filesystem::path("C:/game/Data/WarGame/PC");

ChunkDigest MakeDigest(const char fill) {
	const auto digest = ChunkDigest::FromHex(std::string(64, fill));
	REQUIRE(digest.has_value());
	return *digest;
}

wgrd::domain::PublisherFingerprint MakeFingerprint() {
	const auto fingerprint = wgrd::domain::PublisherFingerprint::FromHex("0011223344556677");
	REQUIRE(fingerprint.has_value());
	return *fingerprint;
}

ModManifest MakeManifest(std::vector<ManifestFile> files, const std::uint64_t version) {
	return ModManifest(MakeFingerprint(), "mod", version, std::move(files));
}

class MapCatalogue final : public IHeldChunkCatalogue {
public:
	~MapCatalogue() override = default;

	void Add(const ChunkDigest& digest, HeldChunkLocation location) {
		_locations.try_emplace(digest.ToHex(), std::move(location));
	}

	[[nodiscard]] std::optional<HeldChunkLocation> Locate(const ChunkDigest& digest) const override {
		const auto match = _locations.find(digest.ToHex());
		if (match == _locations.end()) {
			return std::nullopt;
		}

		return match->second;
	}

private:
	std::map<std::string, HeldChunkLocation> _locations;
};
}

TEST_CASE("diff omits a file whose layout is unchanged") {
	const ManifestFile stableFile{
		"stable.dat", 200,
		{ManifestChunk{MakeDigest('a'), 0, 100}, ManifestChunk{MakeDigest('b'), 100, 100}}
	};

	const ManifestFile heldEdited{
		"edited.dat", 100,
		{ManifestChunk{MakeDigest('c'), 0, 100}}
	};

	const ManifestFile targetEdited{
		"edited.dat", 100,
		{ManifestChunk{MakeDigest('d'), 0, 100}}
	};

	const ModManifest held = MakeManifest({stableFile, heldEdited}, 1);
	const ModManifest target = MakeManifest({stableFile, targetEdited}, 2);

	const ManifestDiffer differ;
	const EmptyChunkCatalogue none;
	const InstallPlan plan = differ.Diff(held, HELD_ROOT, target, none);

	REQUIRE(plan.Files().size() == 1);
	REQUIRE(plan.Files()[0].path == "edited.dat");
	REQUIRE(plan.Files()[0].placements.size() == 1);
	REQUIRE(plan.Files()[0].placements[0].source == ChunkSourceKind::Remote);
	REQUIRE(plan.Removals().empty());
}

TEST_CASE("diff plans every file on a cold install") {
	const ManifestFile firstFile{
		"first.dat", 100,
		{ManifestChunk{MakeDigest('a'), 0, 100}}
	};

	const ManifestFile secondFile{
		"second.dat", 100,
		{ManifestChunk{MakeDigest('b'), 0, 100}}
	};

	const ManifestDiffer differ;
	const EmptyChunkCatalogue none;
	const InstallPlan plan = differ.Diff(ModManifest(), HELD_ROOT, MakeManifest({firstFile, secondFile}, 1), none);

	REQUIRE(plan.Files().size() == 2);
	REQUIRE(plan.RemoteChunkCount() == 2);
}

TEST_CASE("diff plans a file whose chunks moved") {
	const ManifestFile heldFile{
		"moved.dat", 200,
		{ManifestChunk{MakeDigest('a'), 0, 100}, ManifestChunk{MakeDigest('b'), 100, 100}}
	};

	const ManifestFile targetFile{
		"moved.dat", 200,
		{ManifestChunk{MakeDigest('b'), 0, 100}, ManifestChunk{MakeDigest('a'), 100, 100}}
	};

	const ManifestDiffer differ;
	const EmptyChunkCatalogue none;
	const InstallPlan plan = differ.Diff(MakeManifest({heldFile}, 1), HELD_ROOT, MakeManifest({targetFile}, 2), none);

	REQUIRE(plan.Files().size() == 1);
	REQUIRE(plan.Files()[0].placements.size() == 2);
	REQUIRE(plan.Files()[0].placements[0].source == ChunkSourceKind::Held);
	REQUIRE(plan.Files()[0].placements[0].heldFile == HELD_ROOT / "moved.dat");
	REQUIRE(plan.Files()[0].placements[0].heldOffset == 100);
	REQUIRE(plan.Files()[0].placements[1].source == ChunkSourceKind::Held);
	REQUIRE(plan.RemoteChunkCount() == 0);
}

TEST_CASE("diff sources a cold install from the catalogue") {
	const ManifestFile targetFile{
		"48574/ZZ_Win.dat", 300,
		{
			ManifestChunk{MakeDigest('a'), 0, 100},
			ManifestChunk{MakeDigest('b'), 100, 100},
			ManifestChunk{MakeDigest('c'), 200, 100}
		}
	};

	MapCatalogue catalogue;
	catalogue.Add(MakeDigest('a'), HeldChunkLocation{BASELINE_ROOT / "48574/ZZ_Win.dat", 0, 100});
	catalogue.Add(MakeDigest('c'), HeldChunkLocation{BASELINE_ROOT / "48574/ZZ_1.dat", 4096, 100});

	const ManifestDiffer differ;
	const InstallPlan plan = differ.Diff(ModManifest(), HELD_ROOT, MakeManifest({targetFile}, 1), catalogue);

	REQUIRE(plan.Files().size() == 1);
	REQUIRE(plan.Files()[0].placements.size() == 3);

	REQUIRE(plan.Files()[0].placements[0].source == ChunkSourceKind::Held);
	REQUIRE(plan.Files()[0].placements[0].heldFile == BASELINE_ROOT / "48574/ZZ_Win.dat");
	REQUIRE(plan.Files()[0].placements[0].heldOffset == 0);

	REQUIRE(plan.Files()[0].placements[1].source == ChunkSourceKind::Remote);

	REQUIRE(plan.Files()[0].placements[2].source == ChunkSourceKind::Held);
	REQUIRE(plan.Files()[0].placements[2].heldFile == BASELINE_ROOT / "48574/ZZ_1.dat");
	REQUIRE(plan.Files()[0].placements[2].heldOffset == 4096);

	REQUIRE(plan.RemoteChunkCount() == 1);
	REQUIRE(plan.HeldBytes() == 200);
}

TEST_CASE("diff prefers the installed folder over the catalogue") {
	const ManifestFile heldFile{
		"pack.dat", 100,
		{ManifestChunk{MakeDigest('a'), 0, 100}}
	};

	MapCatalogue catalogue;
	catalogue.Add(MakeDigest('a'), HeldChunkLocation{BASELINE_ROOT / "pack.dat", 0, 100});

	const ManifestFile targetFile{
		"pack.dat", 200,
		{ManifestChunk{MakeDigest('a'), 0, 100}, ManifestChunk{MakeDigest('b'), 100, 100}}
	};

	const ManifestDiffer differ;
	const InstallPlan plan = differ.Diff(MakeManifest({heldFile}, 1), HELD_ROOT, MakeManifest({targetFile}, 2), catalogue);

	REQUIRE(plan.Files().size() == 1);
	REQUIRE(plan.Files()[0].placements[0].source == ChunkSourceKind::Held);
	REQUIRE(plan.Files()[0].placements[0].heldFile == HELD_ROOT / "pack.dat");
}

TEST_CASE("diff ignores a catalogue hit whose length disagrees") {
	const ManifestFile targetFile{
		"pack.dat", 100,
		{ManifestChunk{MakeDigest('a'), 0, 100}}
	};

	MapCatalogue catalogue;
	catalogue.Add(MakeDigest('a'), HeldChunkLocation{BASELINE_ROOT / "pack.dat", 0, 50});

	const ManifestDiffer differ;
	const InstallPlan plan = differ.Diff(ModManifest(), HELD_ROOT, MakeManifest({targetFile}, 1), catalogue);

	REQUIRE(plan.Files()[0].placements[0].source == ChunkSourceKind::Remote);
}

TEST_CASE("T10 removal keeps stable") {
	const ManifestFile removedFile{
		"removed.dat", 100,
		{ManifestChunk{MakeDigest('a'), 0, 100}}
	};
	const ManifestFile stableFile{
		"stable.dat", 100,
		{ManifestChunk{MakeDigest('b'), 0, 100}}
	};

	const ManifestDiffer differ;
	const EmptyChunkCatalogue none;
	const InstallPlan plan = differ.Diff(
		MakeManifest({removedFile, stableFile}, 1),
		HELD_ROOT,
		MakeManifest({stableFile}, 2),
		none
	);

	REQUIRE(plan.Files().empty());
	REQUIRE(plan.Removals() == std::vector<std::string>{"removed.dat"});
}

TEST_CASE("T10 reuse repeats placements") {
	const ManifestFile heldFile{
		"held.dat", 100,
		{ManifestChunk{MakeDigest('a'), 0, 100}}
	};
	const ManifestFile firstTarget{
		"first.dat", 200,
		{ManifestChunk{MakeDigest('a'), 0, 100}, ManifestChunk{MakeDigest('a'), 100, 100}}
	};
	const ManifestFile secondTarget{
		"second.dat", 100,
		{ManifestChunk{MakeDigest('a'), 0, 100}}
	};

	const ManifestDiffer differ;
	const EmptyChunkCatalogue none;
	const InstallPlan plan = differ.Diff(
		MakeManifest({heldFile}, 1),
		HELD_ROOT,
		MakeManifest({firstTarget, secondTarget}, 2),
		none
	);

	REQUIRE(plan.Files().size() == 2);
	REQUIRE(plan.Files()[0].placements[0].source == ChunkSourceKind::Held);
	REQUIRE(plan.Files()[0].placements[0].heldFile == HELD_ROOT / "held.dat");
	REQUIRE(plan.Files()[0].placements[0].targetOffset == 0);
	REQUIRE(plan.Files()[0].placements[0].heldOffset == 0);
	REQUIRE(plan.Files()[0].placements[1].source == ChunkSourceKind::Held);
	REQUIRE(plan.Files()[0].placements[1].heldFile == HELD_ROOT / "held.dat");
	REQUIRE(plan.Files()[0].placements[1].targetOffset == 100);
	REQUIRE(plan.Files()[0].placements[1].heldOffset == 0);
	REQUIRE(plan.Files()[1].placements[0].source == ChunkSourceKind::Held);
	REQUIRE(plan.Files()[1].placements[0].heldFile == HELD_ROOT / "held.dat");
	REQUIRE(plan.Files()[1].placements[0].targetOffset == 0);
	REQUIRE(plan.Files()[1].placements[0].heldOffset == 0);
	REQUIRE(plan.HeldBytes() == 300);
	REQUIRE(plan.RemoteBytes() == 0);
}

TEST_CASE("T10 shrink reaches empty") {
	const ManifestFile heldFile{
		"shrinking.dat", 200,
		{ManifestChunk{MakeDigest('a'), 0, 100}, ManifestChunk{MakeDigest('b'), 100, 100}}
	};
	const ManifestFile shortTarget{
		"shrinking.dat", 100,
		{ManifestChunk{MakeDigest('a'), 0, 100}}
	};
	const ManifestFile emptyTarget{"shrinking.dat", 0, {}};

	const ManifestDiffer differ;
	const EmptyChunkCatalogue none;
	const InstallPlan shortPlan = differ.Diff(
		MakeManifest({heldFile}, 1), HELD_ROOT, MakeManifest({shortTarget}, 2), none
	);
	const InstallPlan emptyPlan = differ.Diff(
		MakeManifest({shortTarget}, 2), HELD_ROOT, MakeManifest({emptyTarget}, 3), none
	);

	REQUIRE(shortPlan.Files().size() == 1);
	REQUIRE(shortPlan.Files()[0].path == "shrinking.dat");
	REQUIRE(shortPlan.Files()[0].size == 100);
	REQUIRE(shortPlan.Files()[0].placements.size() == 1);
	REQUIRE(shortPlan.Files()[0].placements[0].targetOffset == 0);
	REQUIRE(shortPlan.HeldBytes() == 100);
	REQUIRE(emptyPlan.Files().size() == 1);
	REQUIRE(emptyPlan.Files()[0].path == "shrinking.dat");
	REQUIRE(emptyPlan.Files()[0].size == 0);
	REQUIRE(emptyPlan.Files()[0].placements.empty());
	REQUIRE(emptyPlan.RemoteBytes() == 0);
	REQUIRE(emptyPlan.HeldBytes() == 0);
}

TEST_CASE("T10 repeated plans match") {
	const ManifestFile heldFile{
		"held.dat", 100,
		{ManifestChunk{MakeDigest('a'), 0, 100}}
	};
	const ManifestFile targetFile{
		"target.dat", 200,
		{ManifestChunk{MakeDigest('a'), 0, 100}, ManifestChunk{MakeDigest('b'), 100, 100}}
	};

	const ManifestDiffer differ;
	const EmptyChunkCatalogue none;
	const ModManifest held = MakeManifest({heldFile}, 1);
	const ModManifest target = MakeManifest({targetFile}, 2);
	const InstallPlan firstPlan = differ.Diff(held, HELD_ROOT, target, none);
	const InstallPlan secondPlan = differ.Diff(held, HELD_ROOT, target, none);

	REQUIRE(firstPlan == secondPlan);
	REQUIRE(firstPlan.Files().size() == 1);
	REQUIRE(firstPlan.Files()[0].path == "target.dat");
	REQUIRE(firstPlan.Files()[0].size == 200);
	REQUIRE(firstPlan.Files()[0].placements.size() == 2);
	REQUIRE(firstPlan.Files()[0].placements[0].targetOffset == 0);
	REQUIRE(firstPlan.Files()[0].placements[0].source == ChunkSourceKind::Held);
	REQUIRE(firstPlan.Files()[0].placements[1].targetOffset == 100);
	REQUIRE(firstPlan.Files()[0].placements[1].source == ChunkSourceKind::Remote);
	REQUIRE(firstPlan.HeldBytes() + firstPlan.RemoteBytes() == 200);
}
