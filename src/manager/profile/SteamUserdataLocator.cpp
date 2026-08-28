#include "manager/profile/SteamUserdataLocator.h"

#include "manager/environment/GameLocator.h"
#include "manager/profile/SteamActiveUser.h"

#include <algorithm>
#include <string>
#include <system_error>

namespace wgrd::manager {
std::vector<domain::SteamAccount> SteamUserdataLocator::Accounts() {
	const std::optional<std::filesystem::path> steamRoot = GameLocator::SteamRoot();
	if (!steamRoot.has_value()) {
		return {};
	}

	const std::optional<std::string> current = SteamActiveUser::Identifier(*steamRoot);

	return AccountsUnder(*steamRoot, current.value_or(std::string()));
}

std::vector<domain::SteamAccount> SteamUserdataLocator::AccountsUnder(
	const std::filesystem::path& steamRoot,
	const std::string_view currentIdentifier
) {
	std::vector<domain::SteamAccount> accounts;

	const std::filesystem::path userdata = steamRoot / USERDATA_FOLDER;

	std::error_code failure;
	if (!std::filesystem::is_directory(userdata, failure)) {
		return accounts;
	}

	std::filesystem::directory_iterator walker(userdata, failure);
	if (failure) {
		return accounts;
	}

	const std::filesystem::directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure || accounts.size() >= ACCOUNT_LIMIT) {
			break;
		}

		if (!walker->is_directory(failure) || failure) {
			continue;
		}

		const std::filesystem::path remote =
				walker->path() / APPLICATION_FOLDER / REMOTE_FOLDER;

		if (!std::filesystem::is_directory(remote, failure) || failure) {
			continue;
		}

		const std::string identifier = walker->path().filename().string();

		accounts.push_back(domain::SteamAccount{
				identifier, remote, !currentIdentifier.empty() && identifier == currentIdentifier
			}
		);
	}

	std::ranges::sort(accounts, [](const domain::SteamAccount& left, const domain::SteamAccount& right) {
			return left.identifier < right.identifier;
		}
	);

	return accounts;
}
}
