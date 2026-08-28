#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wgrd::downloader {

constexpr std::size_t GEAR_TABLE_SIZE = 256;
constexpr std::uint64_t GEAR_TABLE_SEED = 0x1F2E3D4C5B6A7988ULL;

constexpr std::uint64_t AdvanceGearState(std::uint64_t& state) noexcept {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t mixed = state;
    mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ULL;
    mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBULL;
    return mixed ^ (mixed >> 31);
}

constexpr std::array<std::uint64_t, GEAR_TABLE_SIZE> BuildGearTable() noexcept {
    std::array<std::uint64_t, GEAR_TABLE_SIZE> table{};
    std::uint64_t state = GEAR_TABLE_SEED;

    for (std::size_t index = 0; index < GEAR_TABLE_SIZE; ++index) {
        table[index] = AdvanceGearState(state);
    }

    return table;
}

inline constexpr std::array<std::uint64_t, GEAR_TABLE_SIZE> GEAR_TABLE = BuildGearTable();

}
