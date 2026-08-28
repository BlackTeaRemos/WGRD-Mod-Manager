#include "downloader/torrent/build/ChunkSetTorrent.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_info.hpp>

#include <iterator>
#include <system_error>
#include <utility>

namespace wgrd::downloader {
std::expected<ChunkSetTorrentBytes, TorrentBuildError> ChunkSetTorrent::Create(
	const std::filesystem::path& chunkFolder,
	const std::int32_t pieceBytes
) {
	std::error_code discovery;
	if (!std::filesystem::is_directory(chunkFolder, discovery)) {
		return std::unexpected(TorrentBuildError::FolderMissing);
	}

	std::vector<libtorrent::create_file_entry> files =
			libtorrent::list_files(chunkFolder.string());

	if (files.empty()) {
		return std::unexpected(TorrentBuildError::FolderEmpty);
	}

	libtorrent::create_torrent builder(
		std::move(files),
		pieceBytes,
		libtorrent::create_torrent::v2_only
	);

	builder.set_creator("wgrd-mod-manager");
	builder.set_priv(false);

	libtorrent::error_code hashing;
	libtorrent::set_piece_hashes(builder, chunkFolder.parent_path().string(), hashing);
	if (hashing) {
		return std::unexpected(TorrentBuildError::HashingFailed);
	}

	std::vector<char> bencoded;
	libtorrent::bencode(std::back_inserter(bencoded), builder.generate());
	if (bencoded.empty()) {
		return std::unexpected(TorrentBuildError::EncodingFailed);
	}

	libtorrent::error_code parsing;
	const libtorrent::add_torrent_params parameters =
			libtorrent::load_torrent_buffer(bencoded, parsing, libtorrent::load_torrent_limits{});

	if (parsing || parameters.ti == nullptr) {
		return std::unexpected(TorrentBuildError::EncodingFailed);
	}

	const libtorrent::file_storage& storage = parameters.ti->layout();

	std::uint64_t payloadBytes = 0;
	std::size_t payloadFiles = 0;
	std::size_t padFiles = 0;

	for (const libtorrent::file_index_t index : storage.file_range()) {
		if (storage.pad_file_at(index)) {
			++padFiles;
			continue;
		}

		payloadBytes += static_cast<std::uint64_t>(storage.file_size(index));
		++payloadFiles;
	}

	return ChunkSetTorrentBytes{
		std::move(bencoded), libtorrent::aux::to_hex(parameters.info_hashes.v2.to_string()), payloadBytes, payloadFiles, padFiles
	};
}
}
