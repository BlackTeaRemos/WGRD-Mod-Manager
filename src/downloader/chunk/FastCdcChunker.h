#pragma once

#include "domain/interfaces/content/IContentChunker.h"
#include "domain/types/content/ChunkSizes.h"
#include "domain/types/content/ChunkSpan.h"

#include <cstddef>
#include <span>
#include <vector>

namespace wgrd::downloader {
class FastCdcChunker final : public domain::IContentChunker {
public:
	explicit FastCdcChunker(domain::ChunkSizes sizes) noexcept;

	~FastCdcChunker() override;

	[[nodiscard]] std::size_t NextBoundary(std::span<const std::byte> data) const noexcept;

	[[nodiscard]] std::vector<domain::ChunkSpan> Split(std::span<const std::byte> data) const override;

	[[nodiscard]] const domain::ChunkSizes& Sizes() const noexcept;

private:
	domain::ChunkSizes _sizes;
};
}
