#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace wgrd::domain {
struct GameProfileFile {
	std::string account;
	std::string name;
	std::filesystem::path path;
	std::uint64_t sizeBytes = 0;
	bool live = false;

	bool operator==(const GameProfileFile& other) const = default;
};
}
