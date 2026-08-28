#include "manager/patcher/PatcherInstaller.h"
#include "manager/patcher/PatcherMarker.h"
#include "manager/patcher/PatcherRuntimeVersion.h"
#include "manager/update/GitHubReleaseSource.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using wgrd::manager::GitHubReleaseSource;
using wgrd::manager::PatcherInstaller;
using wgrd::manager::PatcherInstallError;
using wgrd::manager::PatcherMarker;
using wgrd::manager::PatcherRuntimeVersion;
using wgrd::manager::ReleaseLookupError;

namespace {
class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_path = std::filesystem::temp_directory_path() / "wgrd-tests" / label;

		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
		std::filesystem::create_directories(_path, failure);
	}

	~TemporaryTree() {
		std::error_code failure;
		std::filesystem::remove_all(_path, failure);
	}

	[[nodiscard]] const std::filesystem::path& Value() const {
		return _path;
	}

private:
	std::filesystem::path _path;
};

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
	std::error_code failure;
	std::filesystem::create_directories(path.parent_path(), failure);

	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output << contents;
}

constexpr std::string_view PATCHER_RELEASE = R"({
  "tag_name": "v2.0.0",
  "assets": [
    {"name": "install.bat", "browser_download_url": "https://github.com/BlackTeaRemos/WRG-Patcher/releases/download/v2.0.0/install.bat", "size": 1839},
    {"name": "version.dll", "browser_download_url": "https://github.com/BlackTeaRemos/WRG-Patcher/releases/download/v2.0.0/version.dll", "size": 433152}
  ]
})";
}

TEST_CASE("release parser selects the asset it was asked for") {
	const auto proxy = GitHubReleaseSource::ParseRelease(PATCHER_RELEASE, "version.dll");

	REQUIRE(proxy.has_value());
	REQUIRE(proxy->tag == "v2.0.0");
	REQUIRE(proxy->assetBytes == 433152);
	REQUIRE(proxy->assetUrl == "https://github.com/BlackTeaRemos/WRG-Patcher/releases/download/v2.0.0/version.dll");

	const auto script = GitHubReleaseSource::ParseRelease(PATCHER_RELEASE, "install.bat");

	REQUIRE(script.has_value());
	REQUIRE(script->assetBytes == 1839);

	const auto absent = GitHubReleaseSource::ParseRelease(PATCHER_RELEASE, "wgrd-mod-manager.exe");

	REQUIRE_FALSE(absent.has_value());
	REQUIRE(absent.error() == ReleaseLookupError::AssetMissing);
}

TEST_CASE("installer refuses a folder without the game executable") {
	const TemporaryTree tree("patchernogame");

	WriteFile(tree.Value() / "staged.dll", "MZproxy");

	const auto installed = PatcherInstaller::Install(tree.Value(), tree.Value() / "staged.dll");

	REQUIRE_FALSE(installed.has_value());
	REQUIRE(installed.error() == PatcherInstallError::GameMissing);
}

TEST_CASE("installer refuses a missing or empty proxy") {
	const TemporaryTree tree("patchernoproxy");

	WriteFile(tree.Value() / "WarGame3.exe", "MZgame");

	const auto missing = PatcherInstaller::Install(tree.Value(), tree.Value() / "absent.dll");

	REQUIRE_FALSE(missing.has_value());
	REQUIRE(missing.error() == PatcherInstallError::StagedMissing);

	WriteFile(tree.Value() / "empty.dll", "");

	const auto empty = PatcherInstaller::Install(tree.Value(), tree.Value() / "empty.dll");

	REQUIRE_FALSE(empty.has_value());
	REQUIRE(empty.error() == PatcherInstallError::StagedEmpty);
}

TEST_CASE("installer stages the proxy the forward and the mods folder") {
	const TemporaryTree tree("patcherinstall");

	WriteFile(tree.Value() / "WarGame3.exe", "MZgame");
	WriteFile(tree.Value() / "staged.dll", "MZproxy");

	REQUIRE_FALSE(PatcherInstaller::Installed(tree.Value()));

	const auto installed = PatcherInstaller::Install(tree.Value(), tree.Value() / "staged.dll");

	REQUIRE(installed.has_value());
	REQUIRE(PatcherInstaller::Installed(tree.Value()));
	REQUIRE(std::filesystem::is_regular_file(tree.Value() / "version_real.dll"));
	REQUIRE(std::filesystem::is_directory(tree.Value() / "mods"));

	std::ifstream proxy(tree.Value() / "version.dll", std::ios::binary);
	const std::string contents(
		(std::istreambuf_iterator<char>(proxy)),
		std::istreambuf_iterator<char>()
	);

	REQUIRE(contents == "MZproxy");
}

TEST_CASE("system library prefers the thirty two bit copy") {
	const std::filesystem::path system = PatcherInstaller::SystemLibrary();

	REQUIRE_FALSE(system.empty());
	REQUIRE(system.filename().string() == "version.dll");
	REQUIRE(system.parent_path().filename().string() == "SysWOW64");
}

TEST_CASE("runtime stamp is read from the mods folder") {
	const TemporaryTree tree("patcherstamp");

	REQUIRE(PatcherRuntimeVersion::Read(tree.Value()).empty());

	WriteFile(tree.Value() / "patcher_version.txt", "1ca4400-6f9dca58\r\n");

	REQUIRE(PatcherRuntimeVersion::Read(tree.Value()) == "1ca4400-6f9dca58");
}

TEST_CASE("runtime stamp refuses binary or oversized content") {
	const TemporaryTree tree("patcherstampbad");

	WriteFile(tree.Value() / "patcher_version.txt", std::string("\x01\x02\x03", 3));
	REQUIRE(PatcherRuntimeVersion::Read(tree.Value()).empty());

	WriteFile(
		tree.Value() / "patcher_version.txt",
		std::string(PatcherRuntimeVersion::LENGTH_LIMIT + 1, 'x')
	);
	REQUIRE(PatcherRuntimeVersion::Read(tree.Value()).empty());
}

TEST_CASE("marker round trips the installed tag") {
	const TemporaryTree tree("patchermarker");

	const PatcherMarker marker(tree.Value());

	REQUIRE(marker.Read().empty());
	REQUIRE(marker.Write("v2.0.0"));
	REQUIRE(marker.Read() == "v2.0.0");

	REQUIRE_FALSE(marker.Write(""));
	REQUIRE_FALSE(marker.Write(std::string(PatcherMarker::TAG_LIMIT + 1, 'x')));
	REQUIRE(marker.Read() == "v2.0.0");
}
