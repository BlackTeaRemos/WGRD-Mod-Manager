#include "manager/tests/ux/OrderHistoryFixture.h"
#include "manager/preferences/PortableWindowPreferences.h"

#include <catch2/catch_test_macros.hpp>
#include <fstream>

TEST_CASE("window preferences stay portable") {
	const wgrd::manager::tests::OrderHistoryFixture fixture;
	const auto folder = fixture.Root() / "Mods" / ".wgrdmm";
	const wgrd::manager::PortableWindowPreferences store(folder);

	SECTION("preferences round trip") {
		wgrd::domain::WindowPreferences preferences;
		preferences.left = -1200;
		preferences.top = 35;
		preferences.width = 1100;
		preferences.height = 800;
		preferences.dpi = 144;
		preferences.positioned = true;
		preferences.maximized = true;
		preferences.backgroundOnClose = true;
		REQUIRE(store.Save(preferences));
		const auto restored = store.Load();
		REQUIRE(restored.left == -1200);
		REQUIRE(restored.width == 1100);
		REQUIRE(restored.dpi == 144);
		REQUIRE(restored.maximized);
		REQUIRE(restored.backgroundOnClose);
	}

	SECTION("invalid data uses defaults") {
		std::filesystem::create_directories(folder);
		std::ofstream output(folder / "window.json");
		output << "{}";
		output.close();
		REQUIRE_FALSE(store.Load().positioned);
		REQUIRE_FALSE(store.Load().backgroundOnClose);
	}

	SECTION("oversized data refused") {
		std::filesystem::create_directories(folder);
		std::ofstream output(folder / "window.json");
		output << std::string(4097, ' ');
		output.close();
		REQUIRE_FALSE(store.Load().positioned);
	}
}
