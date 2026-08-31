#include "downloader/transfer/SeedTorrentAssembler.h"

#include "downloader/torrent/build/VirtualChunkSetTorrent.h"
#include "downloader/torrent/chunkset/ChunkSetLayout.h"

#include <libtorrent/error_code.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_info.hpp>

#include <fstream>
#include <iterator>
#include <utility>

namespace wgrd::downloader {
bool SeedTorrentAssembler::DescribedMatches_(
	const domain::ModManifest& manifest,
	const std::size_t sealedBytes,
	const ChunkSetTorrentBytes& described
) {
	const std::vector<ChunkSetEntry> entries = ChunkSetLayout::Describe(manifest);

	const std::uint64_t expectedBytes =
			ChunkSetLayout::TotalBytes(entries) + static_cast<std::uint64_t>(sealedBytes);

	return described.payloadFiles == entries.size() + 1
	       && described.payloadBytes == expectedBytes;
}

std::vector<std::uint8_t> SeedTorrentAssembler::ReadSealed_(
	const std::filesystem::path& sealedManifestPath
) {
	std::vector<std::uint8_t> sealed;

	std::ifstream input(sealedManifestPath, std::ios::binary);
	if (input) {
		sealed.assign(
			std::istreambuf_iterator<char>(input),
			std::istreambuf_iterator<char>()
		);
	}

	return sealed;
}

std::expected<ChunkSetTorrentBytes, TorrentBuildError> SeedTorrentAssembler::Assemble_(
	const TorrentCache& torrents,
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder,
	const std::vector<std::uint8_t>& sealed,
	const std::string& cacheKey
) {
	if (const auto cached = torrents.Load(cacheKey)) {
		auto described = VirtualChunkSetTorrent::Describe(*cached);
		if (described.has_value() && DescribedMatches_(manifest, sealed.size(), *described)) {
			return described;
		}
	}

	auto built = VirtualChunkSetTorrent::Create(
		manifest,
		modFolder,
		manifest.TorrentName(),
		sealed
	);

	if (built.has_value()) {
		const auto stored = torrents.Save(cacheKey, built->bencoded);
		(void)stored;
	}

	return built;
}

std::optional<PreparedSeedTorrent> SeedTorrentAssembler::Prepare(
	const TorrentCache& torrents,
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder,
	const std::filesystem::path& sealedManifestPath,
	const std::filesystem::path& savePath
) {
	const std::vector<std::uint8_t> sealed = ReadSealed_(sealedManifestPath);

	if (sealed.empty()) {
		return std::nullopt;
	}

	const std::string cacheKey = sealedManifestPath.stem().string();

	const auto torrent = Assemble_(torrents, manifest, modFolder, sealed, cacheKey);

	if (!torrent.has_value()) {
		return std::nullopt;
	}

	libtorrent::error_code parsing;
	libtorrent::add_torrent_params parameters = libtorrent::load_torrent_buffer(
		libtorrent::span<const char>(
			torrent->bencoded.data(),
			static_cast<std::ptrdiff_t>(torrent->bencoded.size())
		),
		parsing,
		libtorrent::load_torrent_limits{}
	);

	if (parsing || parameters.ti == nullptr) {
		return std::nullopt;
	}

	parameters.save_path = savePath.string();
	parameters.flags |= libtorrent::torrent_flags::seed_mode;
	parameters.flags |= libtorrent::torrent_flags::duplicate_is_error;
	parameters.flags &= ~libtorrent::torrent_flags::paused;
	parameters.flags &= ~libtorrent::torrent_flags::auto_managed;

	return PreparedSeedTorrent{
		std::move(parameters),
		torrent->infoHash,
		torrent->payloadBytes,
		sealed.size()
	};
}
}
