#pragma once

#include "domain/types/content/ChunkDigest.h"
#include "domain/types/content/HeldChunkLocation.h"

#include <optional>

namespace wgrd::domain {
class IHeldChunkCatalogue {
public:
	virtual ~IHeldChunkCatalogue() = 0;

	[[nodiscard]] virtual std::optional<HeldChunkLocation> Locate(const ChunkDigest& digest) const = 0;
};

inline IHeldChunkCatalogue::~IHeldChunkCatalogue() = default;
}
