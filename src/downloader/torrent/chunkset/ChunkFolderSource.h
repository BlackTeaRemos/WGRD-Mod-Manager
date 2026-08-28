#pragma once

#include "domain/interfaces/content/IChunkSource.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

namespace wgrd::downloader {
class ChunkFolderSource final : public domain::IChunkSource {
public:
	explicit ChunkFolderSource(std::filesystem::path chunkFolder);

	~ChunkFolderSource() override;

	[[nodiscard]] std::expected<std::vector<std::byte>, domain::ChunkFetchError> Fetch(
		const domain::ChunkDigest& digest,
		std::uint32_t length
	) override;

private:
	std::filesystem::path _chunkFolder;
};
}
