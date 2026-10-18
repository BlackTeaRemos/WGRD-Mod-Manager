#include "../../../tests/support/TemporaryTree.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <string>

namespace wgrd::tests {
namespace {
std::atomic<std::size_t> nextIdentifier = 0;
}

TemporaryTree::TemporaryTree(const std::string_view label) {
	const std::filesystem::path base = std::filesystem::temp_directory_path() / "wgrd-tests";
	std::error_code failure;
	std::filesystem::create_directories(base, failure);
	if (failure) {
		throw std::runtime_error("temporary tree setup failed");
	}

	for (;;) {
		const std::string name = std::string(label) + "-" + std::to_string(nextIdentifier.fetch_add(1));
		const std::filesystem::path candidate = base / name;
		if (std::filesystem::create_directory(candidate, failure)) {
			_root = candidate;
			return;
		}
		if (failure && failure != std::errc::file_exists) {
			throw std::runtime_error("temporary tree setup failed");
		}
		failure.clear();
	}
}

TemporaryTree::~TemporaryTree() noexcept {
	if (!_preserve) {
		std::error_code failure;
		std::filesystem::remove_all(_root, failure);
	}
}

const std::filesystem::path& TemporaryTree::Root() const {
	return _root;
}

void TemporaryTree::Preserve() {
	_preserve = true;
}

void TemporaryTree::WriteFile(const std::string_view relativePath, const std::size_t length, const unsigned char seed) const {
	const std::filesystem::path target = _root / relativePath;
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);
	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	for (std::size_t position = 0; position < length; ++position) {
		output.put(static_cast<char>((position + seed) & 0xFF));
	}
}
}

TEST_CASE("T01 temporary trees isolate labels") {
	const wgrd::tests::TemporaryTree first("same");
	const wgrd::tests::TemporaryTree second("same");

	REQUIRE(first.Root() != second.Root());
	first.WriteFile("sentinel", 1, 1);
	second.WriteFile("sentinel", 1, 2);
	REQUIRE(std::filesystem::exists(first.Root() / "sentinel"));
	REQUIRE(std::filesystem::exists(second.Root() / "sentinel"));
}
