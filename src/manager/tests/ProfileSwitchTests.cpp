#include "manager/profile/GameProfileScanner.h"
#include "manager/profile/GameProfileVault.h"
#include "manager/service/OrderService.h"
#include "manager/service/ProfileService.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using wgrd::domain::InstallFolder;
using wgrd::domain::SteamAccount;
using wgrd::manager::GameProfileScanner;
using wgrd::manager::GameProfileVault;
using wgrd::manager::ProfileService;

namespace {
constexpr std::string_view ACCOUNT = "73266764";

class TemporaryTree {
public:
	explicit TemporaryTree(const std::string_view label) {
		_root = std::filesystem::temp_directory_path() / "wgrd-profile-switch" / label;

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

InstallFolder Folder(const std::string_view name) {
	const auto folder = InstallFolder::Parse(name);
	REQUIRE(folder.has_value());
	return *folder;
}

wgrd::domain::GameInstallation MakeInstallation(const std::filesystem::path& root) {
	const std::filesystem::path mods = root / "Mods";

	std::error_code failure;
	std::filesystem::create_directories(mods / "alpha_mod", failure);
	std::filesystem::create_directories(mods / "gamma_mod", failure);

	return wgrd::domain::GameInstallation{root, mods, mods / "load_order.txt"};
}

std::filesystem::path LivePath(const std::filesystem::path& remote) {
	return remote / (std::string(GameProfileScanner::LIVE_NAME) + std::string(GameProfileScanner::FILE_EXTENSION));
}

void WriteGameProfile(const std::filesystem::path& target, const std::string_view decks) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	std::ofstream output(target, std::ios::binary | std::ios::trunc);
	output << GameProfileVault::MAGIC << decks;
}

std::string ReadWhole(const std::filesystem::path& source) {
	std::ifstream input(source, std::ios::binary);

	return std::string(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);
}
}

TEST_CASE("switching away keeps the live game profile with its own profile") {
	const TemporaryTree tree("keep-on-switch");

	const std::filesystem::path remote = tree.Root() / "userdata" / std::string(ACCOUNT);
	const std::filesystem::path live = LivePath(remote);

	WriteGameProfile(live, "default decks");

	std::vector<SteamAccount> accounts;
	accounts.push_back(SteamAccount{std::string(ACCOUNT), remote, true});

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, accounts);

	REQUIRE(profiles.Active() == ProfileService::DEFAULT_NAME);

	order.SetEnabled(Folder("alpha_mod"), true);
	REQUIRE(profiles.CaptureCurrent("aicclassic").has_value());
	REQUIRE(profiles.Activate("aicclassic").has_value());

	WriteGameProfile(live, "mod decks");

	REQUIRE(profiles.Activate(ProfileService::DEFAULT_NAME).has_value());

	REQUIRE(ReadWhole(live).find("default decks") != std::string::npos);

	REQUIRE(profiles.Activate("aicclassic").has_value());

	INFO("live after returning " << ReadWhole(live));
	REQUIRE(ReadWhole(live).find("mod decks") != std::string::npos);
}

TEST_CASE("switching without a live game profile still succeeds") {
	const TemporaryTree tree("keep-without-live");

	const std::filesystem::path remote = tree.Root() / "userdata" / std::string(ACCOUNT);

	std::vector<SteamAccount> accounts;
	accounts.push_back(SteamAccount{std::string(ACCOUNT), remote, true});

	wgrd::manager::OrderService order(MakeInstallation(tree.Root()));
	ProfileService profiles(tree.Root() / "profiles", order, accounts);

	REQUIRE(profiles.CaptureCurrent("ladder").has_value());
	REQUIRE(profiles.Activate("ladder").has_value());
	REQUIRE(profiles.Activate(ProfileService::DEFAULT_NAME).has_value());
}
