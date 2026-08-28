#include "downloader/chunk/FastCdcChunker.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

using wgrd::domain::ChunkSizes;
using wgrd::domain::ChunkSpan;
using wgrd::downloader::FastCdcChunker;

namespace {
constexpr std::size_t TEST_MINIMUM = 1u << 10;
constexpr std::size_t TEST_AVERAGE = 1u << 12;
constexpr std::size_t TEST_MAXIMUM = 1u << 14;
constexpr std::size_t SAMPLE_LENGTH = 1u << 20;

std::vector<std::byte> MakeSample(const std::size_t length, const std::uint64_t seed) {
	std::vector<std::byte> data;
	data.reserve(length);

	std::uint64_t state = seed;
	for (std::size_t index = 0; index < length; ++index) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		data.push_back(static_cast<std::byte>((state >> 33) & 0xFF));
	}

	return data;
}

ChunkSizes TestSizes() {
	const auto sizes = ChunkSizes::Create(TEST_MINIMUM, TEST_AVERAGE, TEST_MAXIMUM);
	REQUIRE(sizes.has_value());
	return *sizes;
}

std::vector<std::byte> ContentOf(std::span<const std::byte> data, const ChunkSpan& span) {
	const auto begin = data.begin() + static_cast<std::ptrdiff_t>(span.offset);
	return std::vector<std::byte>(begin, begin + static_cast<std::ptrdiff_t>(span.length));
}
}

TEST_CASE("default sizes are sane") {
	const ChunkSizes sizes = ChunkSizes::Default();

	REQUIRE(sizes.Minimum() < sizes.Average());
	REQUIRE(sizes.Average() < sizes.Maximum());
	REQUIRE(sizes.SmallMask() > sizes.LargeMask());
}

TEST_CASE("rejects bad averages") {
	REQUIRE_FALSE(ChunkSizes::Create(1024, 5000, 16384).has_value());
	REQUIRE_FALSE(ChunkSizes::Create(1024, 64, 16384).has_value());
	REQUIRE_FALSE(ChunkSizes::Create(8192, 4096, 16384).has_value());
	REQUIRE_FALSE(ChunkSizes::Create(1024, 4096, 4096).has_value());
}

TEST_CASE("spans cover input exactly") {
	const std::vector<std::byte> data = MakeSample(SAMPLE_LENGTH, 1);
	const FastCdcChunker chunker(TestSizes());

	const std::vector<ChunkSpan> spans = chunker.Split(data);

	REQUIRE_FALSE(spans.empty());
	REQUIRE(spans.front().offset == 0);

	std::size_t expected = 0;
	for (const ChunkSpan& span : spans) {
		REQUIRE(span.offset == expected);
		REQUIRE(span.length > 0);
		expected += span.length;
	}

	REQUIRE(expected == data.size());
}

TEST_CASE("chunking is deterministic") {
	const std::vector<std::byte> data = MakeSample(SAMPLE_LENGTH, 2);
	const FastCdcChunker chunker(TestSizes());

	REQUIRE(chunker.Split(data) == chunker.Split(data));
}

TEST_CASE("respects size bounds") {
	const std::vector<std::byte> data = MakeSample(SAMPLE_LENGTH, 3);
	const FastCdcChunker chunker(TestSizes());

	const std::vector<ChunkSpan> spans = chunker.Split(data);

	for (std::size_t index = 0; index + 1 < spans.size(); ++index) {
		REQUIRE(spans[index].length >= TEST_MINIMUM);
		REQUIRE(spans[index].length <= TEST_MAXIMUM);
	}
}

TEST_CASE("produces many chunks") {
	const std::vector<std::byte> data = MakeSample(SAMPLE_LENGTH, 4);
	const FastCdcChunker chunker(TestSizes());

	REQUIRE(chunker.Split(data).size() > 8);
}

TEST_CASE("short input is one chunk") {
	const std::vector<std::byte> data = MakeSample(TEST_MINIMUM / 2, 5);
	const FastCdcChunker chunker(TestSizes());

	const std::vector<ChunkSpan> spans = chunker.Split(data);

	REQUIRE(spans.size() == 1);
	REQUIRE(spans.front().length == data.size());
}

TEST_CASE("empty input has no chunks") {
	const FastCdcChunker chunker(TestSizes());

	REQUIRE(chunker.Split({}).empty());
}

TEST_CASE("resyncs after insertion") {
	const std::vector<std::byte> original = MakeSample(SAMPLE_LENGTH, 6);

	std::vector<std::byte> shifted = original;
	shifted.insert(shifted.begin() + 3000, std::byte{0x5A});

	const FastCdcChunker chunker(TestSizes());
	const std::vector<ChunkSpan> originalSpans = chunker.Split(original);
	const std::vector<ChunkSpan> shiftedSpans = chunker.Split(shifted);

	std::size_t shared = 0;
	for (const ChunkSpan& span : originalSpans) {
		const std::vector<std::byte> content = ContentOf(original, span);
		const bool present = std::ranges::any_of(shiftedSpans, [&](const ChunkSpan& candidate) {
				return candidate.length == span.length && ContentOf(shifted, candidate) == content;
			}
		);
		if (present) {
			++shared;
		}
	}

	REQUIRE(shared * 10 > originalSpans.size() * 9);
}

TEST_CASE("appended data keeps prefix") {
	const std::vector<std::byte> original = MakeSample(SAMPLE_LENGTH, 7);

	std::vector<std::byte> extended = original;
	const std::vector<std::byte> tail = MakeSample(4096, 8);
	extended.insert(extended.end(), tail.begin(), tail.end());

	const FastCdcChunker chunker(TestSizes());
	const std::vector<ChunkSpan> originalSpans = chunker.Split(original);
	const std::vector<ChunkSpan> extendedSpans = chunker.Split(extended);

	REQUIRE(originalSpans.size() > 2);
	for (std::size_t index = 0; index + 1 < originalSpans.size(); ++index) {
		REQUIRE(originalSpans[index] == extendedSpans[index]);
	}
}
