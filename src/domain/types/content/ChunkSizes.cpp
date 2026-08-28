#include "domain/types/content/ChunkSizes.h"

namespace wgrd::domain {

namespace {

constexpr std::size_t DEFAULT_MINIMUM = 1u << 20;
constexpr std::size_t DEFAULT_AVERAGE = 1u << 22;
constexpr std::size_t DEFAULT_MAXIMUM = 1u << 24;

constexpr unsigned int SMALL_MASK_EXTRA_BITS = 2;
constexpr unsigned int LARGE_MASK_FEWER_BITS = 2;
constexpr unsigned int MINIMUM_AVERAGE_BITS = 8;

}

std::expected<ChunkSizes, ChunkSizesError> ChunkSizes::Create(
    std::size_t minimum,
    std::size_t average,
    std::size_t maximum) {

    if (average == 0 || (average & (average - 1)) != 0) {
        return std::unexpected(ChunkSizesError::AverageNotPowerOfTwo);
    }
    if (TrailingZeroCount_(average) < MINIMUM_AVERAGE_BITS + LARGE_MASK_FEWER_BITS) {
        return std::unexpected(ChunkSizesError::AverageTooSmall);
    }
    if (minimum == 0 || minimum >= average) {
        return std::unexpected(ChunkSizesError::MinimumNotBelowAverage);
    }
    if (maximum <= average) {
        return std::unexpected(ChunkSizesError::MaximumNotAboveAverage);
    }

    return ChunkSizes(minimum, average, maximum);
}

ChunkSizes ChunkSizes::Default() {
    return ChunkSizes(DEFAULT_MINIMUM, DEFAULT_AVERAGE, DEFAULT_MAXIMUM);
}

std::size_t ChunkSizes::Minimum() const noexcept {
    return _minimum;
}

std::size_t ChunkSizes::Average() const noexcept {
    return _average;
}

std::size_t ChunkSizes::Maximum() const noexcept {
    return _maximum;
}

std::uint64_t ChunkSizes::SmallMask() const noexcept {
    return _smallMask;
}

std::uint64_t ChunkSizes::LargeMask() const noexcept {
    return _largeMask;
}

ChunkSizes::ChunkSizes(std::size_t minimum, std::size_t average, std::size_t maximum) noexcept
    : _minimum(minimum)
    , _average(average)
    , _maximum(maximum)
    , _smallMask(0)
    , _largeMask(0) {

    const unsigned int averageBits = TrailingZeroCount_(average);
    _smallMask = (std::uint64_t{1} << (averageBits + SMALL_MASK_EXTRA_BITS)) - 1;
    _largeMask = (std::uint64_t{1} << (averageBits - LARGE_MASK_FEWER_BITS)) - 1;
}

unsigned int ChunkSizes::TrailingZeroCount_(std::size_t value) noexcept {
    unsigned int count = 0;
    while ((value & 1) == 0 && value != 0) {
        value >>= 1;
        ++count;
    }
    return count;
}

}
