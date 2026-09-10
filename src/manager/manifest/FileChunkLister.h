#pragma once

#include "domain/interfaces/content/IContentChunker.h"
#include "domain/interfaces/content/IContentHasher.h"
#include "domain/types/content/ManifestFile.h"

#include <filesystem>
#include <optional>
#include <string>

namespace wgrd::manager {
class FileChunkLister {
public:
	FileChunkLister(const domain::IContentChunker& chunker, const domain::IContentHasher& hasher);

	[[nodiscard]] std::optional<domain::ManifestFile> Describe(
		const std::filesystem::path& file,
		std::string relativePath
	) const;

private:
	const domain::IContentChunker* _chunker;
	const domain::IContentHasher* _hasher;
};
}
