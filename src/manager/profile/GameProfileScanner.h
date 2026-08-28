#pragma once

#include "domain/types/profile/GameProfileFile.h"
#include "domain/types/profile/SteamAccount.h"

#include <string_view>
#include <vector>

namespace wgrd::manager {
class GameProfileScanner {
public:
	static constexpr std::string_view FILE_EXTENSION = ".wargameprofile";
	static constexpr std::string_view LIVE_NAME = "PROFILE";
	static constexpr std::size_t PROFILE_LIMIT = 128;

	[[nodiscard]] static std::vector<domain::GameProfileFile> Scan(const domain::SteamAccount& account);

	[[nodiscard]] static std::vector<domain::GameProfileFile> ScanAll(
		const std::vector<domain::SteamAccount>& accounts
	);

	[[nodiscard]] static std::vector<domain::SteamAccount> Preferred(
		const std::vector<domain::SteamAccount>& accounts
	);

private:
	GameProfileScanner() = delete;
};
}
