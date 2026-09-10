#include "manager/manifest/FileChunkLister.h"

#include "manager/io/MappedFile.h"

#include <utility>

namespace wgrd::manager {
FileChunkLister::FileChunkLister(
	const domain::IContentChunker& chunker,
	const domain::IContentHasher& hasher
)
	: _chunker(&chunker)
	, _hasher(&hasher) {}

std::optional<domain::ManifestFile> FileChunkLister::Describe(
	const std::filesystem::path& file,
	std::string relativePath
) const {
	const auto mapped = MappedFile::Open(file);
	if (!mapped.has_value()) {
		return std::nullopt;
	}

	const std::span<const std::byte> content = mapped->Data();
	const std::vector<domain::ChunkSpan> spans = _chunker->Split(content);

	std::vector<domain::ManifestChunk> chunks;
	chunks.reserve(spans.size());

	for (const domain::ChunkSpan& span : spans) {
		const std::span<const std::byte> slice = content.subspan(span.offset, span.length);

		chunks.push_back(domain::ManifestChunk{
				_hasher->Hash(slice), static_cast<std::uint64_t>(span.offset), static_cast<std::uint32_t>(span.length)
			}
		);
	}

	return domain::ManifestFile{
		std::move(relativePath), mapped->Size(), std::move(chunks)
	};
}
}
