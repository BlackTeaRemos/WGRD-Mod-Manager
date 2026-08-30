#pragma once

#include "domain/interfaces/content/IChunkSetTorrentBuilder.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>

namespace wgrd::downloader {
class ChunkSetTorrentBuilder final : public domain::IChunkSetTorrentBuilder {
public:
	ChunkSetTorrentBuilder();

	~ChunkSetTorrentBuilder() override;

	[[nodiscard]] std::expected<domain::ChunkSetTorrentDescription, domain::ChunkSetTorrentError> Build(
		const domain::ModManifest& manifest,
		const std::filesystem::path& modFolder,
		std::span<const std::uint8_t> sealedManifest
	) const override;

	[[nodiscard]] static std::string TorrentNameFor(const domain::ModManifest& manifest);
};
}
