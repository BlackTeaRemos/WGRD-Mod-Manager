#pragma once

#include <cstdint>
#include <filesystem>

namespace wgrd::domain {
struct HeldChunkLocation {
	std::filesystem::path file;
	std::uint64_t offset = 0;
	std::uint32_t length = 0;

	bool operator==(const HeldChunkLocation& other) const = default;
};
}
