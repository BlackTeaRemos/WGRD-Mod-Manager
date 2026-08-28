#pragma once

#include "domain/types/profile/SteamAccount.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class SteamUserdataLocator {
public:
	static constexpr std::string_view USERDATA_FOLDER = "userdata";
	static constexpr std::string_view APPLICATION_FOLDER = "251060";
	static constexpr std::string_view REMOTE_FOLDER = "remote";
	static constexpr std::size_t ACCOUNT_LIMIT = 64;

	[[nodiscard]] static std::vector<domain::SteamAccount> Accounts();

	[[nodiscard]] static std::vector<domain::SteamAccount> AccountsUnder(
		const std::filesystem::path& steamRoot,
		std::string_view currentIdentifier = {}
	);

private:
	SteamUserdataLocator() = delete;
};
}
