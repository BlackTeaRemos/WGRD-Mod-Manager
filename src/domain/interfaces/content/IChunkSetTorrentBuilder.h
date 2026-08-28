#pragma once

#include "domain/types/content/ChunkSetTorrentDescription.h"
#include "domain/types/content/ModManifest.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>

namespace wgrd::domain {
enum class ChunkSetTorrentError {
	ManifestEmpty, PayloadUnreadable, EncodingFailed
};

class IChunkSetTorrentBuilder {
public:
	virtual ~IChunkSetTorrentBuilder() = 0;

	[[nodiscard]] virtual std::expected<ChunkSetTorrentDescription, ChunkSetTorrentError> Build(
		const ModManifest& manifest,
		const std::filesystem::path& modFolder,
		std::span<const std::uint8_t> sealedManifest
	) const = 0;
};

inline IChunkSetTorrentBuilder::~IChunkSetTorrentBuilder() = default;
}
