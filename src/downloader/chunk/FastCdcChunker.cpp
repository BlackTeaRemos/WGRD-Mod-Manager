#include "downloader/chunk/FastCdcChunker.h"

#include "downloader/chunk/GearTable.h"

#include <algorithm>

namespace wgrd::downloader {
FastCdcChunker::FastCdcChunker(domain::ChunkSizes sizes) noexcept
	: _sizes(sizes) {}

FastCdcChunker::~FastCdcChunker() = default;

std::size_t FastCdcChunker::NextBoundary(const std::span<const std::byte> data) const noexcept {
	const std::size_t available = data.size();
	if (available <= _sizes.Minimum()) {
		return available;
	}

	const std::size_t limit = std::min(available, _sizes.Maximum());
	const std::size_t normalPoint = std::min(limit, _sizes.Average());

	std::uint64_t fingerprint = 0;
	std::size_t position = _sizes.Minimum();

	while (position < normalPoint) {
		fingerprint = (fingerprint >> 1) + GEAR_TABLE[std::to_integer<std::uint8_t>(data[position])];
		if ((fingerprint & _sizes.SmallMask()) == 0) {
			return position + 1;
		}
		++position;
	}

	while (position < limit) {
		fingerprint = (fingerprint >> 1) + GEAR_TABLE[std::to_integer<std::uint8_t>(data[position])];
		if ((fingerprint & _sizes.LargeMask()) == 0) {
			return position + 1;
		}
		++position;
	}

	return limit;
}

std::vector<domain::ChunkSpan> FastCdcChunker::Split(const std::span<const std::byte> data) const {
	std::vector<domain::ChunkSpan> spans;

	std::size_t offset = 0;
	while (offset < data.size()) {
		const std::size_t length = NextBoundary(data.subspan(offset));
		spans.push_back(domain::ChunkSpan{offset, length});
		offset += length;
	}

	return spans;
}

const domain::ChunkSizes& FastCdcChunker::Sizes() const noexcept {
	return _sizes;
}
}
