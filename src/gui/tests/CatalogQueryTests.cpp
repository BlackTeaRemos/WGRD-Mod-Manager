#include "gui/screens/catalog/CatalogQuery.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("catalog queries compose correctly") {
	using wgrd::domain::CatalogRow;
	using wgrd::gui::CatalogQuery;
	using wgrd::gui::CatalogSort;
	const std::vector<CatalogRow> rows = {
		CatalogRow{"0011223344556677/beta", "beta", "Ada", 2, 100, 1, 1, true, true, 1},
		CatalogRow{"1122334455667788/alpha", "Alpha", "Boris", 1, 200, 1, 1, false, true},
		CatalogRow{"2233445566778899/gamma", "gamma", "Ada", 1, 900, 1, 1, false, false}
	};
	SECTION("publisher search respects filters") {
		const auto matches = CatalogQuery::Select(rows, "ADA", 1, CatalogSort::Name);
		REQUIRE(matches.size() == 1);
		REQUIRE(matches.front()->modName == "beta");
	}
	SECTION("updates sort first") {
		const auto matches = CatalogQuery::Select(rows, "", 0, CatalogSort::Updates);
		REQUIRE(matches.front()->Outdated());
	}
	SECTION("unknown sizes sort last") {
		const auto matches = CatalogQuery::Select(rows, "", 0, CatalogSort::Size);
		REQUIRE(matches.front()->modName == "Alpha");
		REQUIRE(matches.back()->modName == "gamma");
	}
	SECTION("names ignore case") {
		const auto matches = CatalogQuery::Select(rows, "", 0, CatalogSort::Name);
		REQUIRE(matches.front()->modName == "Alpha");
		REQUIRE(matches[1]->modName == "beta");
	}
}
