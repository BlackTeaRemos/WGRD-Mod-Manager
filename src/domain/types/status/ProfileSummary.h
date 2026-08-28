#pragma once

#include <string>

namespace wgrd::domain {
struct ProfileSummary {
	std::string name;
	std::size_t entryCount = 0;
	std::size_t enabledCount = 0;
	std::size_t missingCount = 0;
	bool active = false;
	std::string account;
	bool gameProfileHeld = false;

	bool operator==(const ProfileSummary& other) const = default;
};
}
