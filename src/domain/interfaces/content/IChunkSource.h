#pragma once

#include "domain/types/content/ChunkDigest.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace wgrd::domain {
enum class ChunkFetchError {
	Unavailable, LengthMismatch, Timeout
};

class IChunkSource {
public:
	virtual ~IChunkSource() = 0;

	[[nodiscard]] virtual std::expected<std::vector<std::byte>, ChunkFetchError> Fetch(
		const ChunkDigest& digest,
		std::uint32_t length
	) = 0;
};

inline IChunkSource::~IChunkSource() = default;
}
