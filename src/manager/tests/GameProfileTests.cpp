#include "manager/profile/GameProfileScanner.h"
#include "manager/profile/GameProfileVault.h"
#include "manager/profile/SteamActiveUser.h"
#include "manager/profile/SteamUserdataLocator.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using wgrd::manager::GameProfileScanner;
using wgrd::manager::GameProfileVault;
using wgrd::manager::GameProfileVaultError;
using wgrd::manager::SteamActiveUser;
using wgrd::manager::SteamUserdataLocator;

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

std::string ReadFile(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);
	return std::string(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);
}

std::filesystem::path RemoteFolderFor(const std::filesystem::path& steamRoot, const std::string& account) {
	return steamRoot
	       / std::string(SteamUserdataLocator::USERDATA_FOLDER)
	       / account
	       / std::string(SteamUserdataLocator::APPLICATION_FOLDER)
	       / std::string(SteamUserdataLocator::REMOTE_FOLDER);
}
}

TEST_CASE("userdata locator finds every account with a remote folder") {
	const TemporaryTree tree("userdatascan");

	WriteFile(RemoteFolderFor(tree.Value(), "111") / "PROFILE.wargameprofile", "ESAVdata");
	WriteFile(RemoteFolderFor(tree.Value(), "222") / "PROFILE.wargameprofile", "ESAVdata");

	std::error_code failure;
	std::filesystem::create_directories(
		tree.Value() / std::string(SteamUserdataLocator::USERDATA_FOLDER) / "333",
		failure
	);

	const auto accounts = SteamUserdataLocator::AccountsUnder(tree.Value());

	REQUIRE(accounts.size() == 2);
	REQUIRE(accounts[0].identifier == "111");
	REQUIRE(accounts[1].identifier == "222");
}

TEST_CASE("scanner lists profiles and marks the live one") {
	const TemporaryTree tree("profilescan");

	const std::filesystem::path remote = RemoteFolderFor(tree.Value(), "111");

	WriteFile(remote / "PROFILE.wargameprofile", "ESAVlive");
	WriteFile(remote / "vanilla.wargameprofile", "ESAVvanilla");
	WriteFile(remote / "PrivacyPolicy.txt", "ignored");

	const auto accounts = SteamUserdataLocator::AccountsUnder(tree.Value());
	REQUIRE(accounts.size() == 1);

	const auto profiles = GameProfileScanner::Scan(accounts.front());

	REQUIRE(profiles.size() == 2);
	REQUIRE(profiles[0].name == "PROFILE");
	REQUIRE(profiles[0].live);
	REQUIRE(profiles[1].name == "vanilla");
	REQUIRE_FALSE(profiles[1].live);
	REQUIRE(profiles[0].account == "111");
}

TEST_CASE("universe identifier converts to an account folder name") {
	REQUIRE(SteamActiveUser::AccountIdentifierOf("76561198033532492") == "73266764");
	REQUIRE_FALSE(SteamActiveUser::AccountIdentifierOf("notanumber").has_value());
	REQUIRE_FALSE(SteamActiveUser::AccountIdentifierOf("12").has_value());
}

TEST_CASE("login records pick the newest timestamp") {
	const TemporaryTree tree("loginrecords");

	WriteFile(
		tree.Value() / "config" / "loginusers.vdf",
		"\"users\"\n{\n"
		"\t\"76561198321568141\"\n\t{\n\t\t\"AccountName\"\t\"older\"\n\t\t\"Timestamp\"\t\"1705474025\"\n\t}\n"
		"\t\"76561198033532492\"\n\t{\n\t\t\"AccountName\"\t\"newer\"\n\t\t\"Timestamp\"\t\"1787829389\"\n\t}\n"
		"}\n"
	);

	REQUIRE(SteamActiveUser::FromLoginRecords(tree.Value()) == "73266764");
}

TEST_CASE("locator marks only the current account") {
	const TemporaryTree tree("currentaccount");

	WriteFile(RemoteFolderFor(tree.Value(), "111") / "PROFILE.wargameprofile", "ESAVone");
	WriteFile(RemoteFolderFor(tree.Value(), "222") / "PROFILE.wargameprofile", "ESAVtwo");

	const auto accounts = SteamUserdataLocator::AccountsUnder(tree.Value(), "222");

	REQUIRE(accounts.size() == 2);
	REQUIRE_FALSE(accounts[0].current);
	REQUIRE(accounts[1].current);

	const auto preferred = GameProfileScanner::Preferred(accounts);

	REQUIRE(preferred.size() == 1);
	REQUIRE(preferred.front().identifier == "222");
}

TEST_CASE("preferred falls back to every account when none is current") {
	const TemporaryTree tree("noaccount");

	WriteFile(RemoteFolderFor(tree.Value(), "111") / "PROFILE.wargameprofile", "ESAVone");
	WriteFile(RemoteFolderFor(tree.Value(), "222") / "PROFILE.wargameprofile", "ESAVtwo");

	const auto accounts = SteamUserdataLocator::AccountsUnder(tree.Value());

	REQUIRE(GameProfileScanner::Preferred(accounts).size() == 2);
}

TEST_CASE("vault refuses a file without the game magic") {
	const TemporaryTree tree("vaultmagic");

	const std::filesystem::path foreign = tree.Value() / "foreign.wargameprofile";
	WriteFile(foreign, "NOPEnothing");

	REQUIRE_FALSE(GameProfileVault::CarriesMagic(foreign));

	const auto captured = GameProfileVault::Capture(foreign, tree.Value() / "stored.wargameprofile");

	REQUIRE_FALSE(captured.has_value());
	REQUIRE(captured.error() == GameProfileVaultError::SourceForeign);
}

TEST_CASE("vault captures and restores through a backup") {
	const TemporaryTree tree("vaultround");

	const std::filesystem::path live = tree.Value() / "remote" / "PROFILE.wargameprofile";
	const std::filesystem::path stored = tree.Value() / "profiles" / "default.wargameprofile";

	WriteFile(live, "ESAVoriginal");

	REQUIRE(GameProfileVault::Capture(live, stored).has_value());
	REQUIRE(ReadFile(stored) == "ESAVoriginal");

	WriteFile(live, "ESAVreplaced");

	REQUIRE(GameProfileVault::Restore(stored, live).has_value());

	REQUIRE(ReadFile(live) == "ESAVoriginal");

	const std::filesystem::path backup =
			live.string() + std::string(GameProfileVault::BACKUP_SUFFIX);

	REQUIRE(std::filesystem::is_regular_file(backup));
	REQUIRE(ReadFile(backup) == "ESAVreplaced");
}

TEST_CASE("vault reports a missing source") {
	const TemporaryTree tree("vaultmissing");

	const auto restored = GameProfileVault::Restore(
		tree.Value() / "absent.wargameprofile",
		tree.Value() / "live.wargameprofile"
	);

	REQUIRE_FALSE(restored.has_value());
	REQUIRE(restored.error() == GameProfileVaultError::SourceMissing);
}
