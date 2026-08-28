#pragma once

#include "domain/types/content/ManifestChunk.h"

#include <cstdint>
#include <string>
#include <vector>

namespace wgrd::domain {
struct ManifestFile {
	std::string path;
	std::uint64_t size;
	std::vector<ManifestChunk> chunks;

	bool operator==(const ManifestFile& other) const = default;
};
}
