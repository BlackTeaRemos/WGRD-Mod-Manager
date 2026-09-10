#include "domain/rules/BaselinePackLayout.h"

#include <catch2/catch_test_macros.hpp>

using wgrd::domain::BaselinePackLayout;

TEST_CASE("pack root sits under Data WarGame PC") {
	const std::filesystem::path root = BaselinePackLayout::PackRoot("C:/Games/WGRD");

	REQUIRE(root == std::filesystem::path("C:/Games/WGRD") / "Data/WarGame/PC");
}

TEST_CASE("counterpart mirrors the payload path under the pack root") {
	const std::filesystem::path counterpart = BaselinePackLayout::CounterpartFor(
		"C:/Games/WGRD",
		"116084/130278/ZZ_1.dat"
	);

	REQUIRE(counterpart == std::filesystem::path("C:/Games/WGRD/Data/WarGame/PC/116084/130278/ZZ_1.dat"));
}
