#include "domain/types/game/GameBuild.h"
#include "domain/types/order/InstallFolder.h"

#include <catch2/catch_test_macros.hpp>

using wgrd::domain::GameBuild;
using wgrd::domain::InstallFolder;
using wgrd::domain::InstallFolderError;

TEST_CASE("accepts plain folder name") {
	const auto folder = InstallFolder::Parse("sandbox-mod");

	REQUIRE(folder.has_value());
	REQUIRE(folder->Value() == "sandbox-mod");
}

TEST_CASE("accepts underscores and digits") {
	REQUIRE(InstallFolder::Parse("test_mod_4").has_value());
	REQUIRE(InstallFolder::Parse("angel_maps").has_value());
}

TEST_CASE("rejects empty name") {
	const auto folder = InstallFolder::Parse("");

	REQUIRE_FALSE(folder.has_value());
	REQUIRE(folder.error() == InstallFolderError::Empty);
}

TEST_CASE("rejects comment marker") {
	const auto folder = InstallFolder::Parse("#disabled");

	REQUIRE_FALSE(folder.has_value());
	REQUIRE(folder.error() == InstallFolderError::CommentMarker);
}

TEST_CASE("rejects path separators") {
	REQUIRE(InstallFolder::Parse("mods/alpha").error() == InstallFolderError::PathSeparator);
	REQUIRE(InstallFolder::Parse("mods\\alpha").error() == InstallFolderError::PathSeparator);
	REQUIRE(InstallFolder::Parse("../escape").error() == InstallFolderError::PathSeparator);
}

TEST_CASE("rejects surrounding whitespace") {
	REQUIRE(InstallFolder::Parse(" alpha").error() == InstallFolderError::LeadingWhitespace);
	REQUIRE(InstallFolder::Parse("alpha ").error() == InstallFolderError::TrailingWhitespace);
}

TEST_CASE("rejects the managers own data directory") {
	const auto folder = InstallFolder::Parse(".wgrdmm");

	REQUIRE_FALSE(folder.has_value());
	REQUIRE(folder.error() == InstallFolderError::HiddenFolder);
}

TEST_CASE("rejects hidden folders") {
	REQUIRE(InstallFolder::Parse(".git").error() == InstallFolderError::HiddenFolder);
	REQUIRE(InstallFolder::Parse(".hidden_mod").error() == InstallFolderError::HiddenFolder);
}

TEST_CASE("rejects relative markers") {
	REQUIRE(InstallFolder::Parse(".").error() == InstallFolderError::RelativeMarker);
	REQUIRE(InstallFolder::Parse("..").error() == InstallFolderError::RelativeMarker);
}

TEST_CASE("accepts dots inside a name") {
	REQUIRE(InstallFolder::Parse("mod.v2").has_value());
	REQUIRE(InstallFolder::Parse("wgrd-unofficial.1.2").has_value());
}

TEST_CASE("compares by value") {
	const auto left = InstallFolder::Parse("alpha");
	const auto right = InstallFolder::Parse("alpha");
	const auto other = InstallFolder::Parse("beta");

	REQUIRE(*left == *right);
	REQUIRE_FALSE(*left == *other);
}

TEST_CASE("parses build numbers") {
	const auto build = GameBuild::Parse("131635");

	REQUIRE(build.has_value());
	REQUIRE(build->Value() == 131635);
	REQUIRE(build->ToText() == "131635");
}

TEST_CASE("rejects non numeric builds") {
	REQUIRE_FALSE(GameBuild::Parse("Maps").has_value());
	REQUIRE_FALSE(GameBuild::Parse("").has_value());
	REQUIRE_FALSE(GameBuild::Parse("131635a").has_value());
	REQUIRE_FALSE(GameBuild::Parse("-5").has_value());
}

TEST_CASE("builds order numerically") {
	const GameBuild older{116084};
	const GameBuild newer{131635};

	REQUIRE(older < newer);
	REQUIRE(newer == GameBuild{131635});
}
