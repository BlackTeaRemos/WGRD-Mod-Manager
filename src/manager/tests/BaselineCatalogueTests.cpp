#include "manager/baseline/BaselineChunkCatalogue.h"
#include "manager/baseline/records/BaselineRecordCodec.h"
#include "manager/baseline/records/BaselineRecordStore.h"
#include "manager/hash/Blake3Hasher.h"

#include <catch2/catch_test_macros.hpp>

#include "manager/baseline/records/FileStampReader.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

using wgrd::domain::ChunkDigest;
using wgrd::domain::FileStamp;
using wgrd::domain::ManifestChunk;
using wgrd::manager::BaselineChunkCatalogue;
using wgrd::manager::BaselineFileRecord;
using wgrd::manager::BaselineRecordCodec;
using wgrd::manager::BaselineRecordStore;
using wgrd::manager::Blake3Hasher;

namespace {
const std::filesystem::path PACK_ROOT = std::filesystem::path("C:/game/Data/WarGame/PC");

ChunkDigest MakeDigest(const char fill) {
	const auto digest = ChunkDigest::FromHex(std::string(64, fill));
	REQUIRE(digest.has_value());
	return *digest;
}

BaselineFileRecord MakeRecord(const std::string& relativePath, const char fill) {
	return BaselineFileRecord{
		relativePath,
		FileStamp{300, 1234567890},
		{
			ManifestChunk{MakeDigest(fill), 0, 100},
			ManifestChunk{MakeDigest(static_cast<char>(fill + 1)), 100, 200}
		}
	};
}

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-baseline" / label;

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
}

TEST_CASE("baseline record survives an encode decode round trip") {
	const BaselineFileRecord record = MakeRecord("48574/ZZ_Win.dat", 'a');

	const auto decoded = BaselineRecordCodec::Decode(BaselineRecordCodec::Encode(record));

	REQUIRE(decoded.has_value());
	REQUIRE(decoded->relativePath == record.relativePath);
	REQUIRE(decoded->stamp == record.stamp);
	REQUIRE(decoded->chunks.size() == 2);
	REQUIRE(decoded->chunks[1].digest == MakeDigest('b'));
	REQUIRE(decoded->chunks[1].offset == 100);
	REQUIRE(decoded->chunks[1].length == 200);
}

TEST_CASE("baseline record codec rejects malformed documents") {
	REQUIRE_FALSE(BaselineRecordCodec::Decode("not json").has_value());
	REQUIRE_FALSE(BaselineRecordCodec::Decode("{}").has_value());
	REQUIRE_FALSE(BaselineRecordCodec::Decode(
		R"({"path":"a.dat","size":1,"modified":1,"chunks":[{"digest":"zz","offset":0,"length":1}]})"
	).has_value());
}

TEST_CASE("baseline record store saves loads and removes by path") {
	const TemporaryTree tree("store");
	const Blake3Hasher hasher;
	const BaselineRecordStore store(tree.Root() / "baseline", hasher);

	REQUIRE(store.Save(MakeRecord("48574/ZZ_Win.dat", 'a')));
	REQUIRE(store.Save(MakeRecord("48574/ZZ_1.dat", 'c')));

	REQUIRE(store.LoadAll().size() == 2);

	REQUIRE(store.Remove("48574/ZZ_Win.dat"));

	const auto remaining = store.LoadAll();
	REQUIRE(remaining.size() == 1);
	REQUIRE(remaining[0].relativePath == "48574/ZZ_1.dat");
}

TEST_CASE("catalogue locates adopted chunks under the pack root") {
	BaselineChunkCatalogue catalogue;

	catalogue.Adopt(PACK_ROOT, MakeRecord("48574/ZZ_Win.dat", 'a'));

	const auto located = catalogue.Locate(MakeDigest('b'));

	REQUIRE(located.has_value());
	REQUIRE(located->file == PACK_ROOT / "48574/ZZ_Win.dat");
	REQUIRE(located->offset == 100);
	REQUIRE(located->length == 200);
	REQUIRE(catalogue.ChunkCount() == 2);
}

TEST_CASE("catalogue keeps the first location for a repeated digest") {
	BaselineChunkCatalogue catalogue;

	catalogue.Adopt(PACK_ROOT, MakeRecord("48574/ZZ_Win.dat", 'a'));
	catalogue.Adopt(PACK_ROOT, MakeRecord("49125/ZZ_Win.dat", 'a'));

	const auto located = catalogue.Locate(MakeDigest('a'));

	REQUIRE(located.has_value());
	REQUIRE(located->file == PACK_ROOT / "48574/ZZ_Win.dat");
	REQUIRE(catalogue.ChunkCount() == 2);
}

TEST_CASE("catalogue forgets exactly the chunks a file owned") {
	BaselineChunkCatalogue catalogue;

	catalogue.Adopt(PACK_ROOT, MakeRecord("48574/ZZ_Win.dat", 'a'));
	catalogue.Adopt(PACK_ROOT, MakeRecord("48574/ZZ_1.dat", 'c'));

	catalogue.Forget("48574/ZZ_Win.dat");

	REQUIRE_FALSE(catalogue.Locate(MakeDigest('a')).has_value());
	REQUIRE_FALSE(catalogue.Locate(MakeDigest('b')).has_value());
	REQUIRE(catalogue.Locate(MakeDigest('c')).has_value());
	REQUIRE(catalogue.ChunkCount() == 2);
}

TEST_CASE("cache rejects invalid ranges") {
	const BaselineFileRecord record = MakeRecord("48574/ZZ_Win.dat", 'a');
	nlohmann::json document = nlohmann::json::parse(BaselineRecordCodec::Encode(record));

	SECTION("offset overflow") {
		document["chunks"][0]["offset"] = std::numeric_limits<std::uint64_t>::max();
	}
	SECTION("length overflow") {
		document["chunks"][0]["length"] = std::uint64_t{1} << 32;
	}
	SECTION("empty chunk") {
		document["chunks"][0]["length"] = 0;
	}
	SECTION("incomplete file") {
		document["chunks"].erase(1);
	}
	SECTION("overlapping ranges") {
		document["chunks"][1]["offset"] = 50;
	}
	SECTION("oversized range") {
		document["chunks"][1]["length"] = 201;
	}
	SECTION("timestamp overflow") {
		document["modified"] = std::numeric_limits<std::uint64_t>::max();
	}
	SECTION("escaping path") {
		document["path"] = "../pack.dat";
	}
	SECTION("nonpack path") {
		document["path"] = "mod.json";
	}

	REQUIRE_FALSE(BaselineRecordCodec::Decode(document.dump()).has_value());
}

TEST_CASE("catalogue retains alternate sources") {
	BaselineChunkCatalogue catalogue;
	catalogue.Adopt(PACK_ROOT, MakeRecord("48574/ZZ_Win.dat", 'a'));
	catalogue.Adopt(PACK_ROOT, MakeRecord("49125/ZZ_Win.dat", 'a'));

	catalogue.Forget("48574/ZZ_Win.dat");

	const auto located = catalogue.Locate(MakeDigest('a'));
	REQUIRE(located.has_value());
	REQUIRE(located->file == PACK_ROOT / "49125/ZZ_Win.dat");

	catalogue.Adopt(PACK_ROOT, MakeRecord("49125/ZZ_Win.dat", 'c'));
	REQUIRE_FALSE(catalogue.Locate(MakeDigest('a')).has_value());
	REQUIRE(catalogue.Locate(MakeDigest('c')).has_value());

	catalogue.Clear();
	REQUIRE(catalogue.ChunkCount() == 0);
}

TEST_CASE("catalogue discards changed sources") {
	const TemporaryTree tree("changed");
	const std::filesystem::path file = tree.Root() / "pack.dat";
	{
		std::ofstream output(file, std::ios::binary);
		output << std::string(300, 'a');
	}

	BaselineFileRecord record = MakeRecord("pack.dat", 'a');
	const auto stamp = wgrd::manager::FileStampReader::Read(file);
	REQUIRE(stamp.has_value());
	record.stamp = *stamp;

	BaselineChunkCatalogue catalogue;
	catalogue.Adopt(tree.Root(), record);
	catalogue.DiscardChanged();
	REQUIRE(catalogue.Locate(MakeDigest('a')).has_value());

	SECTION("changed size") {
		std::filesystem::resize_file(file, 301);
	}
	SECTION("changed timestamp") {
		std::filesystem::last_write_time(file, std::filesystem::last_write_time(file) + std::chrono::seconds(1));
	}
	SECTION("removed file") {
		REQUIRE(std::filesystem::remove(file));
	}

	catalogue.DiscardChanged();
	REQUIRE_FALSE(catalogue.Locate(MakeDigest('a')).has_value());
}

TEST_CASE("cache bounds document reads") {
	const TemporaryTree tree("oversized");
	const Blake3Hasher hasher;
	const BaselineRecordStore store(tree.Root(), hasher);
	const std::filesystem::path file = tree.Root() / "oversized.wgrdb";
	{
		std::ofstream output(file, std::ios::binary);
		output << "{}";
	}
	std::filesystem::resize_file(file, BaselineRecordCodec::DOCUMENT_LIMIT + 1);
	REQUIRE(store.LoadAll().empty());
}
