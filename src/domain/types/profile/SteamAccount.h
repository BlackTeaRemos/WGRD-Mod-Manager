#pragma once

#include <filesystem>
#include <string>

namespace wgrd::domain {
struct SteamAccount {
	std::string identifier;
	std::filesystem::path remoteFolder;
	bool current = false;

	bool operator==(const SteamAccount& other) const = default;
};
}
