#include "manager/profile/SteamActiveUser.h"

#include <Windows.h>

#include <charconv>
#include <fstream>
#include <iterator>
#include <vector>

namespace wgrd::manager {
namespace {
	constexpr std::string_view TIMESTAMP_KEY = "Timestamp";

	std::vector<std::string> QuotedTokens(const std::string& contents) {
		std::vector<std::string> tokens;

		std::size_t cursor = 0;
		while (cursor < contents.size()) {
			const std::size_t opening = contents.find('"', cursor);
			if (opening == std::string::npos) {
				break;
			}

			const std::size_t closing = contents.find('"', opening + 1);
			if (closing == std::string::npos) {
				break;
			}

			tokens.push_back(contents.substr(opening + 1, closing - opening - 1));
			cursor = closing + 1;
		}

		return tokens;
	}

	std::optional<std::uint64_t> ParseNumber(const std::string_view text) {
		std::uint64_t value = 0;

		const auto outcome = std::from_chars(text.data(), text.data() + text.size(), value);
		if (outcome.ec != std::errc() || outcome.ptr != text.data() + text.size()) {
			return std::nullopt;
		}

		return value;
	}
}

std::optional<std::string> SteamActiveUser::AccountIdentifierOf(const std::string_view universeIdentifier) {
	const std::optional<std::uint64_t> parsed = ParseNumber(universeIdentifier);
	if (!parsed.has_value() || *parsed <= IDENTIFIER_BASE) {
		return std::nullopt;
	}

	return std::to_string(*parsed - IDENTIFIER_BASE);
}

std::optional<std::string> SteamActiveUser::FromRegistry() {
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", 0, KEY_READ, &key)
	    != ERROR_SUCCESS) {
		return std::nullopt;
	}

	DWORD value = 0;
	DWORD size = sizeof(value);
	DWORD type = 0;

	const LSTATUS status = RegQueryValueExW(
		key,
		L"ActiveUser",
		nullptr,
		&type,
		reinterpret_cast<LPBYTE>(&value),
		&size
	);

	RegCloseKey(key);

	if (status != ERROR_SUCCESS || type != REG_DWORD || value == 0) {
		return std::nullopt;
	}

	return std::to_string(value);
}

std::optional<std::string> SteamActiveUser::FromLoginRecords(const std::filesystem::path& steamRoot) {
	std::ifstream input(steamRoot / std::filesystem::path(LOGIN_FILE), std::ios::binary);
	if (!input) {
		return std::nullopt;
	}

	const std::string contents(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);

	const std::vector<std::string> tokens = QuotedTokens(contents);

	std::string newestUniverse;
	std::uint64_t newestTimestamp = 0;
	std::string currentUniverse;

	for (std::size_t index = 0; index < tokens.size(); ++index) {
		if (AccountIdentifierOf(tokens[index]).has_value()) {
			currentUniverse = tokens[index];
			continue;
		}

		if (tokens[index] != TIMESTAMP_KEY || index + 1 >= tokens.size()) {
			continue;
		}

		const std::optional<std::uint64_t> stamp = ParseNumber(tokens[index + 1]);
		if (!stamp.has_value() || currentUniverse.empty()) {
			continue;
		}

		if (*stamp > newestTimestamp) {
			newestTimestamp = *stamp;
			newestUniverse = currentUniverse;
		}
	}

	if (newestUniverse.empty()) {
		return std::nullopt;
	}

	return AccountIdentifierOf(newestUniverse);
}

std::optional<std::string> SteamActiveUser::Identifier(const std::filesystem::path& steamRoot) {
	if (const std::optional<std::string> running = FromRegistry()) {
		return running;
	}

	return FromLoginRecords(steamRoot);
}
}
