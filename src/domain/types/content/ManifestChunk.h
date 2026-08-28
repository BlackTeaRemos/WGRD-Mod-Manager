#pragma once

#include "domain/types/content/ChunkDigest.h"

#include <cstdint>

namespace wgrd::domain {
struct ManifestChunk {
	ChunkDigest digest;
	std::uint64_t offset;
	std::uint32_t length;

	bool operator==(const ManifestChunk& other) const noexcept = default;
};
}
