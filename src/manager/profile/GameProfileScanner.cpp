#include "manager/profile/GameProfileScanner.h"

#include <algorithm>
#include <system_error>

namespace wgrd::manager {
std::vector<domain::GameProfileFile> GameProfileScanner::Scan(const domain::SteamAccount& account) {
	std::vector<domain::GameProfileFile> profiles;

	std::error_code failure;
	if (!std::filesystem::is_directory(account.remoteFolder, failure)) {
		return profiles;
	}

	std::filesystem::directory_iterator walker(account.remoteFolder, failure);
	if (failure) {
		return profiles;
	}

	const std::filesystem::directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure || profiles.size() >= PROFILE_LIMIT) {
			break;
		}

		if (!walker->is_regular_file(failure) || failure) {
			continue;
		}

		if (walker->path().extension() != FILE_EXTENSION) {
			continue;
		}

		const std::string stem = walker->path().stem().string();

		profiles.push_back(domain::GameProfileFile{
				account.identifier,
				stem,
				walker->path(),
				static_cast<std::uint64_t>(std::filesystem::file_size(walker->path(), failure)),
				stem == LIVE_NAME
			}
		);
	}

	std::ranges::sort(profiles, [](const domain::GameProfileFile& left, const domain::GameProfileFile& right) {
			if (left.account != right.account) {
				return left.account < right.account;
			}
			return left.name < right.name;
		}
	);

	return profiles;
}

std::vector<domain::SteamAccount> GameProfileScanner::Preferred(
	const std::vector<domain::SteamAccount>& accounts
) {
	std::vector<domain::SteamAccount> preferred;

	for (const domain::SteamAccount& account : accounts) {
		if (account.current) {
			preferred.push_back(account);
		}
	}

	if (preferred.empty()) {
		return accounts;
	}

	return preferred;
}

std::vector<domain::GameProfileFile> GameProfileScanner::ScanAll(
	const std::vector<domain::SteamAccount>& accounts
) {
	std::vector<domain::GameProfileFile> profiles;

	for (const domain::SteamAccount& account : accounts) {
		for (domain::GameProfileFile& profile : Scan(account)) {
			profiles.push_back(std::move(profile));
		}
	}

	return profiles;
}
}
