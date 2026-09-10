#pragma once

#include "domain/interfaces/content/IHeldChunkCatalogue.h"

namespace wgrd::manager {
class EmptyChunkCatalogue final : public domain::IHeldChunkCatalogue {
public:
	EmptyChunkCatalogue();

	~EmptyChunkCatalogue() override;

	[[nodiscard]] std::optional<domain::HeldChunkLocation> Locate(
		const domain::ChunkDigest& digest
	) const override;
};
}
