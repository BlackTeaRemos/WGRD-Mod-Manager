#include "domain/types/distribution/RepositoryUri.h"

#include <catch2/catch_test_macros.hpp>

using wgrd::domain::RepositoryUri;

TEST_CASE("repository uri qualifies a bare slug") {
	REQUIRE(RepositoryUri::Https("BlackTeaRemos/WGRD-Mod-Manager") ==
		"https://github.com/BlackTeaRemos/WGRD-Mod-Manager"
	);
}

TEST_CASE("repository uri keeps a host qualified value single") {
	REQUIRE(RepositoryUri::Https("github.com/BlackTeaRemos/WGRD-Mod-Manager") ==
		"https://github.com/BlackTeaRemos/WGRD-Mod-Manager"
	);

	REQUIRE(RepositoryUri::Https("https://github.com/BlackTeaRemos/WGRD-Mod-Manager") ==
		"https://github.com/BlackTeaRemos/WGRD-Mod-Manager"
	);
}

TEST_CASE("repository uri trims stray separators") {
	REQUIRE(RepositoryUri::Https("github.com/BlackTeaRemos/WRG-Patcher/") ==
		"https://github.com/BlackTeaRemos/WRG-Patcher"
	);
}

TEST_CASE("repository slug drops scheme and host") {
	REQUIRE(RepositoryUri::Slug("https://github.com/BlackTeaRemos/WGRD-Mod-Manager") ==
		"BlackTeaRemos/WGRD-Mod-Manager"
	);

	REQUIRE(RepositoryUri::Slug("BlackTeaRemos/WGRD-Mod-Manager") ==
		"BlackTeaRemos/WGRD-Mod-Manager"
	);
}

TEST_CASE("repository uri stays empty when nothing is configured") {
	REQUIRE(RepositoryUri::Https("").empty());
}
