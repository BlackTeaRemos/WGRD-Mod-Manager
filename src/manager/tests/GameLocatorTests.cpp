#include "manager/environment/GameLocator.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using wgrd::manager::GameLocator;

namespace {
constexpr std::string_view GAME_EXECUTABLE = "WarGame3.exe";

class TemporaryRoot {
public:
	explicit TemporaryRoot(const std::string_view label) {
		_path = std::filesystem::temp_directory_path() / "wgrd-tests" / label;

		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
		std::filesystem::create_directories(_path, failure);
	}

	~TemporaryRoot() {
		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
	}

	[[nodiscard]] const std::filesystem::path& Value() const {
		return _path;
	}

private:
	std::filesystem::path _path;
};

void PlaceExecutable(const std::filesystem::path& root) {
	std::ofstream output(root / std::string(GAME_EXECUTABLE), std::ios::binary | std::ios::trunc);
	output << "MZ";
}
}

TEST_CASE("locator creates the mods folder when the game executable is present") {
	const TemporaryRoot root("locatorcreate");

	PlaceExecutable(root.Value());

	REQUIRE_FALSE(std::filesystem::exists(root.Value() / "Mods"));

	const auto installation = GameLocator::FromRoot(root.Value());

	REQUIRE(installation.has_value());
	REQUIRE(std::filesystem::is_directory(root.Value() / "Mods"));
	REQUIRE(installation->modsDirectory == root.Value() / "Mods");
	REQUIRE(installation->orderFile == root.Value() / "Mods" / "load_order.txt");
}

TEST_CASE("locator keeps an existing mods folder untouched") {
	const TemporaryRoot root("locatorkeep");

	PlaceExecutable(root.Value());

	std::error_code failure;
	std::filesystem::create_directories(root.Value() / "Mods" / "existing_mod", failure);

	const auto installation = GameLocator::FromRoot(root.Value());

	REQUIRE(installation.has_value());
	REQUIRE(std::filesystem::is_directory(root.Value() / "Mods" / "existing_mod"));
}

TEST_CASE("locator refuses a folder without the game executable") {
	const TemporaryRoot root("locatorbare");

	std::error_code failure;
	std::filesystem::create_directories(root.Value() / "Mods", failure);

	REQUIRE_FALSE(GameLocator::FromRoot(root.Value()).has_value());
}

TEST_CASE("locator refuses a folder holding only a directory named like the executable") {
	const TemporaryRoot root("locatordirectory");

	std::error_code failure;
	std::filesystem::create_directories(root.Value() / std::string(GAME_EXECUTABLE), failure);

	REQUIRE_FALSE(GameLocator::FromRoot(root.Value()).has_value());
}
