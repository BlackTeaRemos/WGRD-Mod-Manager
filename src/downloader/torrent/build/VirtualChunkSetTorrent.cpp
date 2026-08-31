#include "downloader/torrent/build/VirtualChunkSetTorrent.h"

#include "downloader/torrent/build/ChunkMerkleHasher.h"
#include "downloader/torrent/chunkset/ChunkSetLayout.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/torrent_info.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace wgrd::downloader {
namespace {
	struct SourceRange {
		std::string path;
		std::uint64_t offset;
	};

	std::map<std::string, SourceRange> IndexSources(const domain::ModManifest& manifest) {
		std::map<std::string, SourceRange> index;

		for (const domain::ManifestFile& file : manifest.Files()) {
			for (const domain::ManifestChunk& chunk : file.chunks) {
				index.try_emplace(chunk.digest.ToHex(), SourceRange{file.path, chunk.offset});
			}
		}

		return index;
	}

	bool ReadRange(
		const std::filesystem::path& source,
		const std::uint64_t offset,
		const std::uint32_t length,
		std::vector<std::byte>& target
	) {
		std::ifstream input(source, std::ios::binary);
		if (!input) {
			return false;
		}

		input.seekg(static_cast<std::streamoff>(offset));

		target.resize(length);
		input.read(reinterpret_cast<char*>(target.data()), length);

		return input.gcount() == static_cast<std::streamsize>(length);
	}
}

std::expected<ChunkSetTorrentBytes, TorrentBuildError> VirtualChunkSetTorrent::Create(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder,
	std::string_view torrentName,
	std::span<const std::uint8_t> sealedManifest,
	std::int32_t pieceBytes
) {
	const std::vector<ChunkSetEntry> entries = ChunkSetLayout::Describe(manifest);
	if (entries.empty()) {
		return std::unexpected(TorrentBuildError::FolderEmpty);
	}

	const std::map<std::string, SourceRange> sources = IndexSources(manifest);

	std::vector<libtorrent::create_file_entry> files;
	files.reserve(entries.size());

	for (const ChunkSetEntry& entry : entries) {
		files.emplace_back(
			std::string(torrentName) + "/" + entry.fileName,
			static_cast<std::int64_t>(entry.length)
		);
	}

	const std::string manifestPath =
			std::string(torrentName) + "/" + std::string(domain::ChunkFileNaming::MANIFEST_FILE);

	if (!sealedManifest.empty()) {
		files.emplace_back(manifestPath, static_cast<std::int64_t>(sealedManifest.size()));
	}

	libtorrent::create_torrent builder(
		std::move(files),
		pieceBytes,
		libtorrent::create_torrent::v2_only
	);

	builder.set_creator("wgrd-mod-manager");
	builder.set_priv(false);

	const std::size_t blocksPerPiece =
			static_cast<std::size_t>(pieceBytes) / ChunkMerkleHasher::BLOCK_BYTES;

	std::map<std::string, libtorrent::file_index_t> placement;
	for (const libtorrent::file_index_t fileIndex : builder.file_list().range()) {
		const libtorrent::create_file_entry& candidate = builder.file_list()[fileIndex];

		if ((candidate.flags & libtorrent::file_storage::flag_pad_file) != libtorrent::file_flags_t{}) {
			continue;
		}

		placement.try_emplace(candidate.filename, fileIndex);
	}

	std::vector<std::byte> chunkBytes;

	for (std::size_t index = 0; index < entries.size(); ++index) {
		const ChunkSetEntry& entry = entries[index];

		const auto located = placement.find(std::string(torrentName) + "/" + entry.fileName);
		if (located == placement.end()) {
			return std::unexpected(TorrentBuildError::HashingFailed);
		}

		const auto source = sources.find(entry.digest.ToHex());
		if (source == sources.end()) {
			return std::unexpected(TorrentBuildError::HashingFailed);
		}

		if (!ReadRange(
			modFolder / source->second.path,
			source->second.offset,
			entry.length,
			chunkBytes
		)) {
			return std::unexpected(TorrentBuildError::HashingFailed);
		}

		const std::size_t pieceCount =
				(static_cast<std::size_t>(entry.length) + static_cast<std::size_t>(pieceBytes) - 1) /
				static_cast<std::size_t>(pieceBytes);

		const std::size_t singlePieceTarget =
				ChunkMerkleHasher::LeafTarget(entry.length, pieceBytes);

		for (std::size_t piece = 0; piece < pieceCount; ++piece) {
			const std::size_t begin = piece * static_cast<std::size_t>(pieceBytes);
			const std::size_t length =
					std::min(static_cast<std::size_t>(pieceBytes), chunkBytes.size() - begin);

			const std::size_t leafTarget = pieceCount == 1 ? singlePieceTarget : blocksPerPiece;

			const libtorrent::sha256_hash root = ChunkMerkleHasher::PieceRoot(
				std::span<const std::byte>(chunkBytes).subspan(begin, length),
				leafTarget
			);

			builder.set_hash2(
				located->second,
				static_cast<libtorrent::piece_index_t::diff_type>(static_cast<int>(piece)),
				root
			);
		}
	}

	if (!sealedManifest.empty()) {
		const auto located = placement.find(manifestPath);
		if (located == placement.end()) {
			return std::unexpected(TorrentBuildError::HashingFailed);
		}

		const std::span<const std::byte> manifestBytes(
			reinterpret_cast<const std::byte*>(sealedManifest.data()),
			sealedManifest.size()
		);

		const std::size_t pieceCount =
				(sealedManifest.size() + static_cast<std::size_t>(pieceBytes) - 1) /
				static_cast<std::size_t>(pieceBytes);

		const std::size_t singlePieceTarget =
				ChunkMerkleHasher::LeafTarget(sealedManifest.size(), pieceBytes);

		for (std::size_t piece = 0; piece < pieceCount; ++piece) {
			const std::size_t begin = piece * static_cast<std::size_t>(pieceBytes);
			const std::size_t length =
					std::min(static_cast<std::size_t>(pieceBytes), manifestBytes.size() - begin);

			const std::size_t leafTarget = pieceCount == 1 ? singlePieceTarget : blocksPerPiece;

			builder.set_hash2(
				located->second,
				static_cast<libtorrent::piece_index_t::diff_type>(static_cast<int>(piece)),
				ChunkMerkleHasher::PieceRoot(manifestBytes.subspan(begin, length), leafTarget)
			);
		}
	}

	std::vector<char> bencoded;
	libtorrent::bencode(std::back_inserter(bencoded), builder.generate());

	return Describe(std::move(bencoded));
}

std::expected<ChunkSetTorrentBytes, TorrentBuildError> VirtualChunkSetTorrent::Describe(
	std::vector<char> bencoded
) {
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

	for (const libtorrent::file_index_t fileIndex : storage.file_range()) {
		if (storage.pad_file_at(fileIndex)) {
			++padFiles;
			continue;
		}

		payloadBytes += static_cast<std::uint64_t>(storage.file_size(fileIndex));
		++payloadFiles;
	}

	return ChunkSetTorrentBytes{
		std::move(bencoded), libtorrent::aux::to_hex(parameters.info_hashes.v2.to_string()), payloadBytes, payloadFiles, padFiles
	};
}
}
