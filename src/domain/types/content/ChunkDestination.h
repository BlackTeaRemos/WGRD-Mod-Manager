#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace wgrd::domain {
struct ChunkDestination {
	std::string chunkFileName;
	std::filesystem::path file;
	std::uint64_t offset = 0;
	std::uint32_t length = 0;

	bool operator==(const ChunkDestination& other) const = default;
};
}
