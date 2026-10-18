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

TEST_CASE("T31 empty query returns all rows") {
	using wgrd::domain::CatalogRow;
	using wgrd::gui::CatalogQuery;
	using wgrd::gui::CatalogSort;
	const std::vector<CatalogRow> rows = {
		CatalogRow{"0011223344556677/beta", "beta", "Ada", 2, 100, 1, 1, true, true, 1},
		CatalogRow{"1122334455667788/alpha", "Alpha", "Boris", 1, 200, 1, 1, false, true},
		CatalogRow{"2233445566778899/gamma", "gamma", "Ada", 1, 900, 1, 1, false, false}
	};

	const auto matches = CatalogQuery::Select(rows, "", 0, CatalogSort::Name);

	REQUIRE(matches.size() == rows.size());
}

TEST_CASE("T31 exact query matches identity fields") {
	using wgrd::domain::CatalogRow;
	using wgrd::gui::CatalogQuery;
	using wgrd::gui::CatalogSort;
	const std::vector<CatalogRow> rows = {
		CatalogRow{"0011223344556677/beta", "beta", "Ada", 2, 100, 1, 1, true, true, 1},
		CatalogRow{"1122334455667788/alpha", "Alpha", "Boris", 1, 200, 1, 1, false, true},
		CatalogRow{"2233445566778899/gamma", "gamma", "Ada", 1, 900, 1, 1, false, false}
	};

	const auto nameMatches = CatalogQuery::Select(rows, "Alpha", 0, CatalogSort::Name);
	const auto identifierMatches = CatalogQuery::Select(rows, "7788/alpha", 0, CatalogSort::Name);

	REQUIRE(nameMatches.size() == 1);
	REQUIRE(nameMatches.front()->identifier == "1122334455667788/alpha");
	REQUIRE(identifierMatches.size() == 1);
	REQUIRE(identifierMatches.front()->identifier == "1122334455667788/alpha");
}

TEST_CASE("T31 filters select installed and held") {
	using wgrd::domain::CatalogRow;
	using wgrd::gui::CatalogQuery;
	using wgrd::gui::CatalogSort;
	const std::vector<CatalogRow> rows = {
		CatalogRow{"0011223344556677/installed", "installed", "Ada", 1, 100, 1, 1, true, true},
		CatalogRow{"1122334455667788/held", "held", "Boris", 1, 200, 1, 1, false, true},
		CatalogRow{"2233445566778899/remote", "remote", "Cara", 1, 300, 1, 1, false, false}
	};

	const auto installed = CatalogQuery::Select(rows, "", 1, CatalogSort::Name);
	const auto held = CatalogQuery::Select(rows, "", 2, CatalogSort::Name);

	REQUIRE(installed.size() == 1);
	REQUIRE(installed.front()->identifier == "0011223344556677/installed");
	REQUIRE(held.size() == 2);
	REQUIRE(held[0]->identifier == "1122334455667788/held");
	REQUIRE(held[1]->identifier == "0011223344556677/installed");
}

TEST_CASE("T31 equal names sort by identifier") {
	using wgrd::domain::CatalogRow;
	using wgrd::gui::CatalogQuery;
	using wgrd::gui::CatalogSort;
	const std::vector<CatalogRow> rows = {
		CatalogRow{"2233445566778899/same", "Same", "Ada", 1, 100, 1, 1, false, true},
		CatalogRow{"0011223344556677/same", "same", "Boris", 1, 100, 1, 1, false, true}
	};

	const auto matches = CatalogQuery::Select(rows, "", 0, CatalogSort::Name);

	REQUIRE(matches.size() == 2);
	REQUIRE(matches[0]->identifier == "0011223344556677/same");
	REQUIRE(matches[1]->identifier == "2233445566778899/same");
}
