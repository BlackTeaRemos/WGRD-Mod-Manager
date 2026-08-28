#pragma once

#include <cstddef>
#include <cstdint>

namespace wgrd::domain::limits {

inline constexpr std::size_t ANNOUNCE_RECORD_BYTES = 256;
inline constexpr std::size_t MANIFEST_ENVELOPE_BYTES = 2 * 1024 * 1024;
inline constexpr std::size_t MANIFEST_CHUNK_COUNT = 16384;
inline constexpr std::size_t VERIFY_ARENA_BYTES = 32 * 1024 * 1024;

inline constexpr std::uint32_t PEER_BURST_ANNOUNCES = 8;
inline constexpr std::uint32_t PEER_ANNOUNCE_REFILL_SECONDS = 60;

}
