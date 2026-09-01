#include "manager/install/ContentInstaller.h"
#include "manager/scan/ModFolderScanner.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

using wgrd::manager::ContentInstaller;
using wgrd::manager::ModFolderScanner;

namespace {
class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-scan" / label;

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

void WriteFile(const std::filesystem::path& target) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output << "payload";
}
}

TEST_CASE("a folder holding only staging files is not installed") {
	const TemporaryTree tree("staging-only");

	const std::filesystem::path abandoned = tree.Root() / "abandoned_mod";

	WriteFile(abandoned / "packs" / ("ZZ_Win.dat" + std::string(ContentInstaller::STAGING_SUFFIX)));

	REQUIRE_FALSE(ModFolderScanner::HoldsPayload(abandoned));
	REQUIRE(ModFolderScanner::Scan(tree.Root()).empty());
}

TEST_CASE("a folder keeps its install once real payload lands") {
	const TemporaryTree tree("staging-and-payload");

	const std::filesystem::path partial = tree.Root() / "partial_mod";

	WriteFile(partial / "packs" / ("ZZ_Win.dat" + std::string(ContentInstaller::STAGING_SUFFIX)));
	WriteFile(partial / "packs" / "ZZ_Data.dat");

	REQUIRE(ModFolderScanner::HoldsPayload(partial));

	const auto scanned = ModFolderScanner::Scan(tree.Root());

	REQUIRE(scanned.size() == 1);
	REQUIRE(scanned[0].folder.Value() == "partial_mod");
}

TEST_CASE("an empty folder stays listed") {
	const TemporaryTree tree("empty");

	std::error_code failure;
	std::filesystem::create_directories(tree.Root() / "hollow_mod", failure);

	REQUIRE(ModFolderScanner::HoldsPayload(tree.Root() / "hollow_mod"));
	REQUIRE(ModFolderScanner::Scan(tree.Root()).size() == 1);
}
