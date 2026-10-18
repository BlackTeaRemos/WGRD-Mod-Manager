#include "manager/hash/Blake3Hasher.h"
#include "manager/install/ContentInstaller.h"

#include "domain/interfaces/content/IChunkSource.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
class UnavailableSource final : public wgrd::domain::IChunkSource {
public:
	~UnavailableSource() override = default;
	std::expected<std::vector<std::byte>, wgrd::domain::ChunkFetchError> Fetch(
		const wgrd::domain::ChunkDigest&, std::uint32_t
	) override {
		return std::unexpected(wgrd::domain::ChunkFetchError::Unavailable);
	}
};
}

TEST_CASE("T11 failed staging preserves target") {
	const auto digest = wgrd::domain::ChunkDigest::FromHex(std::string(64, 'a'));
	REQUIRE(digest.has_value());
	const auto root = std::filesystem::temp_directory_path() / "wgrd-t11-owned";
	std::error_code failure;
	std::filesystem::remove_all(root, failure);
	std::filesystem::create_directories(root, failure);
	const auto target = root / "pack.dat";
	{
		std::ofstream output(target, std::ios::binary | std::ios::trunc);
		output << "original";
	}
	const wgrd::domain::ChunkPlacement placement{*digest, 0, 4, wgrd::domain::ChunkSourceKind::Remote, {}, 0};
	const wgrd::domain::InstallPlan plan({wgrd::domain::FilePlan{"pack.dat", 4, {placement}}}, {});
	wgrd::manager::Blake3Hasher hasher;
	wgrd::manager::ContentInstaller installer(hasher);
	UnavailableSource source;

	const auto result = installer.Apply(plan, root, source);

	REQUIRE_FALSE(result.has_value());
	REQUIRE(result.error() == wgrd::manager::InstallError::RemoteChunkUnavailable);
	std::ifstream input(target, std::ios::binary);
	const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	REQUIRE(contents == "original");
	std::filesystem::remove_all(root, failure);
}
