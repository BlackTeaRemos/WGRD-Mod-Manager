#include "manager/scan/ModMetadataReader.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using wgrd::manager::ModMetadataReader;

namespace {
class TemporaryMod {
public:
	explicit TemporaryMod(const std::string_view label) {
		_path = std::filesystem::temp_directory_path() / "wgrd-tests" / label;

		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
		std::filesystem::create_directories(_path, failure);
	}

	~TemporaryMod() {
		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
	}

	[[nodiscard]] const std::filesystem::path& Value() const {
		return _path;
	}

private:
	std::filesystem::path _path;
};

void WriteManifest(const std::filesystem::path& folder, const std::string& contents) {
	std::ofstream output(folder / "mod.json", std::ios::binary | std::ios::trunc);
	output << contents;
}
}

TEST_CASE("metadata reader parses a toolkit manifest") {
	const TemporaryMod mod("metadataread");

	WriteManifest(mod.Value(), R"({
  "name": "test_mod_7",
  "version": "1.0",
  "author": "BlackTea",
  "description": "adds units",
  "packs": ["131615/Data.dat", "131615/ZZ_1.dat"],
  "built_from_revision": "131544",
  "created_with": "WGRD Mod Toolkit"
})");

	const auto metadata = ModMetadataReader::Read(mod.Value());

	REQUIRE(metadata.present);
	REQUIRE(metadata.name == "test_mod_7");
	REQUIRE(metadata.version == "1.0");
	REQUIRE(metadata.author == "BlackTea");
	REQUIRE(metadata.description == "adds units");
	REQUIRE(metadata.builtFromRevision == "131544");
	REQUIRE(metadata.packCount == 2);
}

TEST_CASE("metadata reader reports absence without a manifest") {
	const TemporaryMod mod("metadataabsent");

	const auto metadata = ModMetadataReader::Read(mod.Value());

	REQUIRE_FALSE(metadata.present);
	REQUIRE(metadata.packCount == 0);
}

TEST_CASE("metadata reader refuses malformed json") {
	const TemporaryMod mod("metadatabroken");

	WriteManifest(mod.Value(), "{ not json");

	REQUIRE_FALSE(ModMetadataReader::Read(mod.Value()).present);
}

TEST_CASE("metadata reader tolerates missing fields") {
	const TemporaryMod mod("metadatapartial");

	WriteManifest(mod.Value(), R"({"name": "bare"})");

	const auto metadata = ModMetadataReader::Read(mod.Value());

	REQUIRE(metadata.present);
	REQUIRE(metadata.name == "bare");
	REQUIRE(metadata.version.empty());
	REQUIRE(metadata.packCount == 0);
}

TEST_CASE("metadata reader clamps an overlong field") {
	const TemporaryMod mod("metadatalong");

	const std::string huge(ModMetadataReader::FIELD_LIMIT + 500, 'x');
	WriteManifest(mod.Value(), R"({"description": ")" + huge + R"("})");

	const auto metadata = ModMetadataReader::Read(mod.Value());

	REQUIRE(metadata.present);
	REQUIRE(metadata.description.size() == ModMetadataReader::FIELD_LIMIT);
}
