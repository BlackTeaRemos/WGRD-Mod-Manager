#include "downloader/torrent/chunkset/ChunkFolderSource.h"

#include "downloader/torrent/chunkset/ChunkSetLayout.h"

#include <fstream>
#include <utility>

namespace wgrd::downloader {
ChunkFolderSource::ChunkFolderSource(std::filesystem::path chunkFolder)
	: _chunkFolder(std::move(chunkFolder)) {}

ChunkFolderSource::~ChunkFolderSource() = default;

std::expected<std::vector<std::byte>, domain::ChunkFetchError> ChunkFolderSource::Fetch(
	const domain::ChunkDigest& digest,
	const std::uint32_t length
) {
	const std::filesystem::path source = _chunkFolder / ChunkSetLayout::FileNameFor(digest);

	std::ifstream input(source, std::ios::binary);
	if (!input) {
		return std::unexpected(domain::ChunkFetchError::Unavailable);
	}

	std::vector<std::byte> bytes(length);
	input.read(reinterpret_cast<char*>(bytes.data()), length);

	if (input.gcount() != static_cast<std::streamsize>(length)) {
		return std::unexpected(domain::ChunkFetchError::LengthMismatch);
	}

	return bytes;
}
}
