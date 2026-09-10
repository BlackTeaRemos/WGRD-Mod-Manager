#include "manager/install/EmptyChunkCatalogue.h"

namespace wgrd::manager {
EmptyChunkCatalogue::EmptyChunkCatalogue() = default;

EmptyChunkCatalogue::~EmptyChunkCatalogue() = default;

std::optional<domain::HeldChunkLocation> EmptyChunkCatalogue::Locate(const domain::ChunkDigest&) const {
	return std::nullopt;
}
}
