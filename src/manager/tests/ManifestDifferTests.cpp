#include "manager/install/ManifestDiffer.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using wgrd::domain::ChunkDigest;
using wgrd::domain::ChunkSourceKind;
using wgrd::domain::InstallPlan;
using wgrd::domain::ManifestChunk;
using wgrd::domain::ManifestFile;
using wgrd::domain::ModManifest;
using wgrd::manager::ManifestDiffer;

namespace {
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
	const InstallPlan plan = differ.Diff(held, target);

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
	const InstallPlan plan = differ.Diff(ModManifest(), MakeManifest({firstFile, secondFile}, 1));

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
	const InstallPlan plan = differ.Diff(MakeManifest({heldFile}, 1), MakeManifest({targetFile}, 2));

	REQUIRE(plan.Files().size() == 1);
	REQUIRE(plan.Files()[0].placements.size() == 2);
	REQUIRE(plan.Files()[0].placements[0].source == ChunkSourceKind::Held);
	REQUIRE(plan.Files()[0].placements[1].source == ChunkSourceKind::Held);
	REQUIRE(plan.RemoteChunkCount() == 0);
}
