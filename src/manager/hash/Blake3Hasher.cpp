#include "manager/hash/Blake3Hasher.h"

#include <blake3.h>

#include <array>

namespace wgrd::manager {
Blake3Hasher::Blake3Hasher() = default;

Blake3Hasher::~Blake3Hasher() = default;

domain::ChunkDigest Blake3Hasher::Hash(const std::span<const std::byte> data) const {
	blake3_hasher hasher;
	blake3_hasher_init(&hasher);
	blake3_hasher_update(&hasher, data.data(), data.size());

	std::array<std::uint8_t, BLAKE3_OUT_LEN> digest{};
	blake3_hasher_finalize(&hasher, digest.data(), digest.size());

	const auto value = domain::ChunkDigest::FromBytes(digest);
	return value.value_or(domain::ChunkDigest{});
}
}
