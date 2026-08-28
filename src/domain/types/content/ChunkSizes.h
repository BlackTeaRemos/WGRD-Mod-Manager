#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>

namespace wgrd::domain {

enum class ChunkSizesError {
    AverageNotPowerOfTwo,
    AverageTooSmall,
    MinimumNotBelowAverage,
    MaximumNotAboveAverage
};

class ChunkSizes {
public:
    static std::expected<ChunkSizes, ChunkSizesError> Create(
        std::size_t minimum,
        std::size_t average,
        std::size_t maximum);

    static ChunkSizes Default();

    [[nodiscard]] std::size_t Minimum() const noexcept;
    [[nodiscard]] std::size_t Average() const noexcept;
    [[nodiscard]] std::size_t Maximum() const noexcept;

    [[nodiscard]] std::uint64_t SmallMask() const noexcept;
    [[nodiscard]] std::uint64_t LargeMask() const noexcept;

    bool operator==(const ChunkSizes& other) const noexcept = default;

private:
    ChunkSizes(std::size_t minimum, std::size_t average, std::size_t maximum) noexcept;

    static unsigned int TrailingZeroCount_(std::size_t value) noexcept;

    std::size_t _minimum;
    std::size_t _average;
    std::size_t _maximum;
    std::uint64_t _smallMask;
    std::uint64_t _largeMask;
};

}
