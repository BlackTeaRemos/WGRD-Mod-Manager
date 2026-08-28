#pragma once

#include "domain/types/content/ChunkDigest.h"

#include <cstddef>
#include <span>

namespace wgrd::domain {
class IContentHasher {
public:
	virtual ~IContentHasher() = 0;

	[[nodiscard]] virtual ChunkDigest Hash(std::span<const std::byte> data) const = 0;
};

inline IContentHasher::~IContentHasher() = default;
}
