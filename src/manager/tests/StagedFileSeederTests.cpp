#include "manager/hash/Blake3Hasher.h"
#include "manager/install/StagedFileSeeder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using wgrd::domain::ChunkPlacement;
using wgrd::domain::ChunkSourceKind;
using wgrd::domain::FilePlan;
using wgrd::manager::Blake3Hasher;
using wgrd::manager::StagedFileSeeder;

namespace {
constexpr std::uint32_t HELD_LENGTH = 2048;
constexpr std::uint64_t FILE_LENGTH = 4096;

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-staged-seed" / label;

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

std::vector<std::byte> MakePattern(const std::size_t length, const std::uint8_t seed) {
	std::vector<std::byte> bytes(length);
	for (std::size_t position = 0; position < length; ++position) {
		bytes[position] = static_cast<std::byte>((position * 31 + seed) & 0xFF);
	}
	return bytes;
}

void WriteBytes(const std::filesystem::path& target, const std::vector<std::byte>& bytes) {
	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::byte> ReadBytes(const std::filesystem::path& source) {
	std::ifstream input(source, std::ios::binary);
	const std::istreambuf_iterator<char> first(input);
	constexpr std::istreambuf_iterator<char> last;
	const std::string raw(first, last);

	std::vector<std::byte> bytes;
	bytes.reserve(raw.size());
	for (const char character : raw) {
		bytes.push_back(static_cast<std::byte>(character));
	}
	return bytes;
}

FilePlan MakePlan(const Blake3Hasher& hasher, const std::vector<std::byte>& original) {
	const auto heldDigest = hasher.Hash(
		std::span<const std::byte>(original.data(), HELD_LENGTH)
	);

	const auto remoteDigest = wgrd::domain::ChunkDigest::FromHex(std::string(64, 'e'));
	REQUIRE(remoteDigest.has_value());

	std::vector<ChunkPlacement> placements;
	placements.push_back(ChunkPlacement{
			heldDigest, 0, HELD_LENGTH, ChunkSourceKind::Held, "pack.dat", 0
		}
	);
	placements.push_back(ChunkPlacement{
			*remoteDigest,
			HELD_LENGTH,
			static_cast<std::uint32_t>(FILE_LENGTH - HELD_LENGTH),
			ChunkSourceKind::Remote,
			std::string(),
			0
		}
	);

	return FilePlan{"pack.dat", FILE_LENGTH, std::move(placements)};
}
}

TEST_CASE("seeding without staging copies the original") {
	const TemporaryTree tree("fresh");
	const Blake3Hasher hasher;

	const std::vector<std::byte> original = MakePattern(FILE_LENGTH, 3);
	WriteBytes(tree.Root() / "pack.dat", original);

	const FilePlan plan = MakePlan(hasher, original);
	const StagedFileSeeder seeder(hasher);

	bool resumed = false;
	REQUIRE(seeder.Seed(tree.Root() / "pack.dat", tree.Root() / "pack.dat.partial", plan, resumed));

	REQUIRE_FALSE(resumed);
	REQUIRE(ReadBytes(tree.Root() / "pack.dat.partial") == original);
}

TEST_CASE("seeding resumes staging whose held ranges still match") {
	const TemporaryTree tree("resume");
	const Blake3Hasher hasher;

	const std::vector<std::byte> original = MakePattern(FILE_LENGTH, 3);
	WriteBytes(tree.Root() / "pack.dat", original);
	WriteBytes(tree.Root() / "pack.dat.partial", original);

	const FilePlan plan = MakePlan(hasher, original);
	const StagedFileSeeder seeder(hasher);

	bool resumed = false;
	REQUIRE(seeder.Seed(tree.Root() / "pack.dat", tree.Root() / "pack.dat.partial", plan, resumed));

	REQUIRE(resumed);
}

TEST_CASE("seeding rejects stale staging and reseeds from the original") {
	const TemporaryTree tree("stale");
	const Blake3Hasher hasher;

	const std::vector<std::byte> original = MakePattern(FILE_LENGTH, 3);
	WriteBytes(tree.Root() / "pack.dat", original);

	std::vector<std::byte> stale = MakePattern(FILE_LENGTH, 90);
	WriteBytes(tree.Root() / "pack.dat.partial", stale);

	const FilePlan plan = MakePlan(hasher, original);
	const StagedFileSeeder seeder(hasher);

	bool resumed = false;
	REQUIRE(seeder.Seed(tree.Root() / "pack.dat", tree.Root() / "pack.dat.partial", plan, resumed));

	REQUIRE_FALSE(resumed);
	REQUIRE(ReadBytes(tree.Root() / "pack.dat.partial") == original);
}
