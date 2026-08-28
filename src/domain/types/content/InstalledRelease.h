#pragma once

#include <cstdint>
#include <string>

namespace wgrd::domain {
struct InstalledRelease {
	std::string identifier;
	std::string modName;
	std::uint64_t version = 0;
	std::string manifestDigest;

	bool operator==(const InstalledRelease& other) const = default;
};
}
