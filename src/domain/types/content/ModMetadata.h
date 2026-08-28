#pragma once

#include <cstddef>
#include <string>

namespace wgrd::domain {
struct ModMetadata {
	bool present = false;
	std::string name;
	std::string version;
	std::string author;
	std::string description;
	std::string builtFromRevision;
	std::string createdWith;
	std::size_t packCount = 0;

	bool operator==(const ModMetadata& other) const = default;
};
}
