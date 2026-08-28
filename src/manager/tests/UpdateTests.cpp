#include "manager/update/DownloadHostPolicy.h"
#include "manager/update/ExecutableSwapper.h"
#include "manager/update/GitHubContentsSource.h"
#include "manager/update/GitHubReleaseSource.h"
#include "manager/update/VersionNumber.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using wgrd::manager::DownloadHostPolicy;
using wgrd::manager::ExecutableSwapper;
using wgrd::manager::GitHubContentsSource;
using wgrd::manager::GitHubReleaseSource;
using wgrd::manager::ReleaseLookupError;
using wgrd::manager::VersionNumber;

namespace {
std::string ReleaseDocument(const std::string_view tag, const std::string_view assetName, const std::string_view url) {
	return std::string(R"({"tag_name":")") + std::string(tag) +
	       R"(","assets":[{"name":")" + std::string(assetName) +
	       R"(","browser_download_url":")" + std::string(url) +
	       R"(","size":1234}]})";
}
}

TEST_CASE("version parses tags with and without a prefix") {
	const auto prefixed = VersionNumber::Parse("v1.4.2");
	const auto bare = VersionNumber::Parse("1.4.2");

	REQUIRE(prefixed.has_value());
	REQUIRE(bare.has_value());
	REQUIRE(*prefixed == *bare);
	REQUIRE(prefixed->Major() == 1);
	REQUIRE(prefixed->Minor() == 4);
	REQUIRE(prefixed->Patch() == 2);
	REQUIRE(prefixed->ToText() == "1.4.2");
}

TEST_CASE("version rejects malformed text") {
	REQUIRE_FALSE(VersionNumber::Parse("").has_value());
	REQUIRE_FALSE(VersionNumber::Parse("1.2").has_value());
	REQUIRE_FALSE(VersionNumber::Parse("1.2.3.4").has_value());
	REQUIRE_FALSE(VersionNumber::Parse("1.2.x").has_value());
	REQUIRE_FALSE(VersionNumber::Parse("v..").has_value());
	REQUIRE_FALSE(VersionNumber::Parse("1.2.999999").has_value());
}

TEST_CASE("version orders by component") {
	REQUIRE(VersionNumber(0, 1, 0) < VersionNumber(0, 1, 1));
	REQUIRE(VersionNumber(0, 1, 9) < VersionNumber(0, 2, 0));
	REQUIRE(VersionNumber(0, 9, 9) < VersionNumber(1, 0, 0));
	REQUIRE(VersionNumber(1, 0, 0) == VersionNumber(1, 0, 0));
	REQUIRE_FALSE(VersionNumber(1, 0, 0) < VersionNumber(0, 9, 9));
}

TEST_CASE("release url is built from a bare or qualified repository") {
	REQUIRE(GitHubReleaseSource::BuildUrl("BlackTeaRemos/WGRD-Mod-Manager") ==
		"https://api.github.com/repos/BlackTeaRemos/WGRD-Mod-Manager/releases/latest"
	);

	REQUIRE(GitHubReleaseSource::BuildUrl("github.com/BlackTeaRemos/WGRD-Mod-Manager") ==
		"https://api.github.com/repos/BlackTeaRemos/WGRD-Mod-Manager/releases/latest"
	);
}

TEST_CASE("release parse finds the bare executable asset") {
	const std::string document = ReleaseDocument(
		"v0.2.0",
		GitHubReleaseSource::ASSET_NAME,
		"https://github.com/owner/repo/releases/download/v0.2.0/wgrd-mod-manager.exe"
	);

	const auto release = GitHubReleaseSource::ParseRelease(document);

	REQUIRE(release.has_value());
	REQUIRE(release->version == VersionNumber(0, 2, 0));
	REQUIRE(release->tag == "v0.2.0");
	REQUIRE(release->assetBytes == 1234);
	REQUIRE(release->assetUrl.starts_with("https://"));
}

TEST_CASE("release parse refuses a zip only release") {
	const std::string document = ReleaseDocument(
		"v0.2.0",
		"wgrd-mod-manager-0.2.0-x64-windows-static.zip",
		"https://github.com/owner/repo/releases/download/v0.2.0/bundle.zip"
	);

	const auto release = GitHubReleaseSource::ParseRelease(document);

	REQUIRE_FALSE(release.has_value());
	REQUIRE(release.error() == ReleaseLookupError::AssetMissing);
}

TEST_CASE("release parse refuses a plain http asset url") {
	const std::string document = ReleaseDocument(
		"v0.2.0",
		GitHubReleaseSource::ASSET_NAME,
		"http://github.com/owner/repo/releases/download/v0.2.0/wgrd-mod-manager.exe"
	);

	const auto release = GitHubReleaseSource::ParseRelease(document);

	REQUIRE_FALSE(release.has_value());
	REQUIRE(release.error() == ReleaseLookupError::AssetUrlRejected);
}

TEST_CASE("release parse refuses malformed and untagged documents") {
	REQUIRE_FALSE(GitHubReleaseSource::ParseRelease("not json").has_value());
	REQUIRE_FALSE(GitHubReleaseSource::ParseRelease(R"({"assets":[]})").has_value());

	const auto badTag = GitHubReleaseSource::ParseRelease(
		R"({"tag_name":"nightly","assets":[]})"
	);

	REQUIRE_FALSE(badTag.has_value());
	REQUIRE(badTag.error() == ReleaseLookupError::TagRejected);
}

TEST_CASE("swapper names staged and retired files beside the executable") {
	const std::filesystem::path current = ExecutableSwapper::CurrentExecutable();

	REQUIRE_FALSE(current.empty());
	REQUIRE(ExecutableSwapper::StagedPath() ==
		std::filesystem::path(current.string() + std::string(ExecutableSwapper::STAGED_SUFFIX))
	);
	REQUIRE(ExecutableSwapper::RetiredPath() ==
		std::filesystem::path(current.string() + std::string(ExecutableSwapper::RETIRED_SUFFIX))
	);
	REQUIRE(ExecutableSwapper::StagedPath().parent_path() == current.parent_path());
}

TEST_CASE("swapper refuses to apply without a staged executable") {
	std::error_code failure;
	std::filesystem::remove(ExecutableSwapper::StagedPath(), failure);

	const auto applied = ExecutableSwapper::Apply();

	REQUIRE_FALSE(applied.has_value());
	REQUIRE(applied.error() == wgrd::manager::SwapError::StagedMissing);
}

TEST_CASE("swapper refuses an empty staged executable") {
	const std::filesystem::path staged = ExecutableSwapper::StagedPath();

	{
		std::ofstream output(staged, std::ios::binary | std::ios::trunc);
	}

	const auto applied = ExecutableSwapper::Apply();

	std::error_code failure;
	std::filesystem::remove(staged, failure);

	REQUIRE_FALSE(applied.has_value());
	REQUIRE(applied.error() == wgrd::manager::SwapError::StagedEmpty);
}

TEST_CASE("registry file names accept only a fingerprint stem") {
	REQUIRE(GitHubContentsSource::IsRegistryFileName("0011223344556677.json"));
	REQUIRE(GitHubContentsSource::IsRegistryFileName("aabbccddeeff0011.json"));

	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName("../evil.json"));
	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName("..\\evil.json"));
	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName("0011223344556677.json.exe"));
	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName("0011223344556677.exe"));
	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName("AABBCCDDEEFF0011.json"));
	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName("001122334455667.json"));
	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName("00112233445566778.json"));
	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName("00112233445566gg.json"));
	REQUIRE_FALSE(GitHubContentsSource::IsRegistryFileName(""));
}

TEST_CASE("contents url is built for a folder") {
	REQUIRE(GitHubContentsSource::BuildUrl("BlackTeaRemos/WGRD-Mod-Manager-Signatures", "keys") ==
		"https://api.github.com/repos/BlackTeaRemos/WGRD-Mod-Manager-Signatures/contents/keys"
	);

	REQUIRE(GitHubContentsSource::BuildUrl("github.com/owner/repo", "revoked") ==
		"https://api.github.com/repos/owner/repo/contents/revoked"
	);
}

TEST_CASE("contents listing keeps only registry files") {
	const std::string document = R"([
        {"type":"file","name":"0011223344556677.json","download_url":"https://raw.githubusercontent.com/a/b/keys/0011223344556677.json","size":210},
        {"type":"file","name":"README.md","download_url":"https://raw.githubusercontent.com/a/b/keys/README.md","size":10},
        {"type":"dir","name":"nested","download_url":null,"size":0},
        {"type":"file","name":"../escape.json","download_url":"https://raw.githubusercontent.com/a/b/escape.json","size":10}
    ])";

	const auto entries = GitHubContentsSource::ParseListing(document);

	REQUIRE(entries.has_value());
	REQUIRE(entries->size() == 1);
	REQUIRE(entries->front().name == "0011223344556677.json");
	REQUIRE(entries->front().bytes == 210);
}

TEST_CASE("contents listing drops an oversized or plain http entry") {
	const std::string document = R"([
        {"type":"file","name":"0011223344556677.json","download_url":"https://raw.githubusercontent.com/a/b/x.json","size":999999},
        {"type":"file","name":"aabbccddeeff0011.json","download_url":"http://raw.githubusercontent.com/a/b/y.json","size":10}
    ])";

	const auto entries = GitHubContentsSource::ParseListing(document);

	REQUIRE(entries.has_value());
	REQUIRE(entries->empty());
}

TEST_CASE("contents listing refuses malformed documents") {
	REQUIRE_FALSE(GitHubContentsSource::ParseListing("not json").has_value());
	REQUIRE_FALSE(GitHubContentsSource::ParseListing(R"({"message":"Not Found"})").has_value());
}

TEST_CASE("host policy accepts only the published github hosts") {
	REQUIRE(DownloadHostPolicy::Accepts("https://github.com/owner/repo/releases/download/v1/a.exe"));
	REQUIRE(DownloadHostPolicy::Accepts("https://api.github.com/repos/owner/repo/releases/latest"));
	REQUIRE(DownloadHostPolicy::Accepts("https://raw.githubusercontent.com/a/b/c.json"));
	REQUIRE(DownloadHostPolicy::Accepts("https://objects.githubusercontent.com/blob"));
	REQUIRE(DownloadHostPolicy::Accepts("https://GitHub.COM/owner/repo"));
}

TEST_CASE("host policy refuses lookalike and credential shaped urls") {
	REQUIRE_FALSE(DownloadHostPolicy::Accepts("https://github.com.evil.example/a.exe"));
	REQUIRE_FALSE(DownloadHostPolicy::Accepts("https://github.com@evil.example/a.exe"));
	REQUIRE_FALSE(DownloadHostPolicy::Accepts("https://evil.example/github.com/a.exe"));
	REQUIRE_FALSE(DownloadHostPolicy::Accepts("https://github.com:8443/a.exe"));
	REQUIRE_FALSE(DownloadHostPolicy::Accepts("http://github.com/a.exe"));
	REQUIRE_FALSE(DownloadHostPolicy::Accepts("https:///a.exe"));
	REQUIRE_FALSE(DownloadHostPolicy::Accepts(""));
}

TEST_CASE("host policy reads the authority up to the first delimiter") {
	REQUIRE(DownloadHostPolicy::HostOf("https://github.com/owner") == "github.com");
	REQUIRE(DownloadHostPolicy::HostOf("https://github.com?a=b") == "github.com");
	REQUIRE(DownloadHostPolicy::HostOf("https://github.com") == "github.com");
	REQUIRE(DownloadHostPolicy::HostOf("ftp://github.com").empty());
}
