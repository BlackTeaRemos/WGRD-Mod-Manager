#include "downloader/torrent/build/ChunkSetTorrentBuilder.h"

#include "downloader/torrent/build/VirtualChunkSetTorrent.h"

#include <libtorrent/hex.hpp>

#include <utility>

namespace wgrd::downloader {
ChunkSetTorrentBuilder::ChunkSetTorrentBuilder() = default;

ChunkSetTorrentBuilder::~ChunkSetTorrentBuilder() = default;

std::string ChunkSetTorrentBuilder::TorrentNameFor(const domain::ModManifest& manifest) {
	return manifest.TorrentName();
}

std::expected<domain::ChunkSetTorrentDescription, domain::ChunkSetTorrentError> ChunkSetTorrentBuilder::Build(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder,
	const std::span<const std::uint8_t> sealedManifest
) const {
	auto built = VirtualChunkSetTorrent::Create(manifest, modFolder, TorrentNameFor(manifest), sealedManifest);

	if (!built.has_value()) {
		if (built.error() == TorrentBuildError::FolderEmpty) {
			return std::unexpected(domain::ChunkSetTorrentError::ManifestEmpty);
		}

		if (built.error() == TorrentBuildError::HashingFailed) {
			return std::unexpected(domain::ChunkSetTorrentError::PayloadUnreadable);
		}

		return std::unexpected(domain::ChunkSetTorrentError::EncodingFailed);
	}

	const auto infoHash = domain::ChunkDigest::FromHex(built->infoHash);
	if (!infoHash.has_value()) {
		return std::unexpected(domain::ChunkSetTorrentError::EncodingFailed);
	}

	return domain::ChunkSetTorrentDescription{
		std::move(built->bencoded), *infoHash, built->payloadBytes, built->payloadFiles
	};
}
}
