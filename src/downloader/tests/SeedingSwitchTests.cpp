#include "downloader/storage/SeedingSwitch.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

using wgrd::downloader::SeedingSwitch;

namespace {
class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-seed-switch" / label;

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

TEST_CASE("seeding preference is absent until it is saved") {
	const TemporaryTree tree("absent");

	SeedingSwitch stored;
	stored.UseFolder(tree.Root());

	REQUIRE_FALSE(stored.Load().has_value());
}

TEST_CASE("seeding preference round trips both states") {
	const TemporaryTree tree("round-trip");

	SeedingSwitch stored;
	stored.UseFolder(tree.Root());

	stored.Save(false);

	const auto paused = stored.Load();

	REQUIRE(paused.has_value());
	REQUIRE_FALSE(*paused);

	stored.Save(true);

	const auto running = stored.Load();

	REQUIRE(running.has_value());
	REQUIRE(*running);
}

TEST_CASE("seeding preference ignores an unreadable mark") {
	const TemporaryTree tree("bad-mark");

	{
		std::ofstream output(tree.Root() / SeedingSwitch::FILE_NAME, std::ios::binary | std::ios::trunc);
		output << "nonsense";
	}

	SeedingSwitch stored;
	stored.UseFolder(tree.Root());

	REQUIRE_FALSE(stored.Load().has_value());
}

TEST_CASE("seeding preference stays quiet without a folder") {
	const SeedingSwitch stored;

	stored.Save(false);

	REQUIRE_FALSE(stored.Load().has_value());
}
