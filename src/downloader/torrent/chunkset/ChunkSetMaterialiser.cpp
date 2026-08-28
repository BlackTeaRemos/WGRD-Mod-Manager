#include "downloader/torrent/chunkset/ChunkSetMaterialiser.h"

#include "downloader/torrent/chunkset/ChunkSetLayout.h"

#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace wgrd::downloader {
namespace {
	struct SourceRange {
		std::string path;
		std::uint64_t offset;
		std::uint32_t length;
	};

	std::map<std::string, SourceRange> IndexSources(const domain::ModManifest& manifest) {
		std::map<std::string, SourceRange> index;

		for (const domain::ManifestFile& file : manifest.Files()) {
			for (const domain::ManifestChunk& chunk : file.chunks) {
				index.try_emplace(chunk.digest.ToHex(), SourceRange{file.path, chunk.offset, chunk.length});
			}
		}

		return index;
	}
}

std::expected<std::size_t, MaterialiseError> ChunkSetMaterialiser::Write(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder,
	const std::filesystem::path& chunkFolder
) {
	std::error_code failure;
	std::filesystem::create_directories(chunkFolder, failure);
	if (failure) {
		return std::unexpected(MaterialiseError::TargetUnwritable);
	}

	const std::map<std::string, SourceRange> sources = IndexSources(manifest);
	const std::vector<ChunkSetEntry> entries = ChunkSetLayout::Describe(manifest);

	std::size_t written = 0;

	for (const ChunkSetEntry& entry : entries) {
		const auto source = sources.find(entry.digest.ToHex());
		if (source == sources.end()) {
			return std::unexpected(MaterialiseError::SourceUnreadable);
		}

		std::ifstream input(modFolder / source->second.path, std::ios::binary);
		if (!input) {
			return std::unexpected(MaterialiseError::SourceUnreadable);
		}

		input.seekg(static_cast<std::streamoff>(source->second.offset));

		std::vector<char> bytes(entry.length);
		input.read(bytes.data(), entry.length);
		if (input.gcount() != static_cast<std::streamsize>(entry.length)) {
			return std::unexpected(MaterialiseError::LengthMismatch);
		}

		std::ofstream output(chunkFolder / entry.fileName, std::ios::binary | std::ios::trunc);
		if (!output) {
			return std::unexpected(MaterialiseError::TargetUnwritable);
		}

		output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		if (!output) {
			return std::unexpected(MaterialiseError::TargetUnwritable);
		}

		++written;
	}

	return written;
}
}
