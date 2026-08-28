#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace wgrd::manager {
class SteamActiveUser {
public:
	static constexpr std::string_view LOGIN_FILE = "config/loginusers.vdf";
	static constexpr std::uint64_t IDENTIFIER_BASE = 76561197960265728ull;

	[[nodiscard]] static std::optional<std::string> Identifier(const std::filesystem::path& steamRoot);

	[[nodiscard]] static std::optional<std::string> FromRegistry();

	[[nodiscard]] static std::optional<std::string> FromLoginRecords(const std::filesystem::path& steamRoot);

	[[nodiscard]] static std::optional<std::string> AccountIdentifierOf(std::string_view universeIdentifier);

private:
	SteamActiveUser() = delete;
};
}
