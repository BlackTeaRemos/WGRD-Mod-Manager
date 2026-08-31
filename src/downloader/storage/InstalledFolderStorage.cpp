#include "downloader/storage/InstalledFolderStorage.h"

#include "downloader/storage/ResumeAttestation.h"
#include "downloader/torrent/build/ChunkMerkleHasher.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/peer_request.hpp>
#include <libtorrent/storage_defs.hpp>

#include <boost/asio/post.hpp>

#include <algorithm>
#include <utility>

namespace wgrd::downloader {
InstalledFolderStorage::InstalledFolderStorage(
	const ChunkLocator& locator,
	libtorrent::io_context& context,
	StorageFaults& faults,
	const SeedAttestations& attestations,
	StorageBacklog& backlog,
	const OpenFileCache& handles
)
	: _locator(&locator)
	, _context(&context)
	, _faults(&faults)
	, _attestations(&attestations)
	, _backlog(&backlog)
	, _rangeIo(locator, handles)
	, _guard()
	, _queue()
	, _torrents() {}

InstalledFolderStorage::~InstalledFolderStorage() {
	_queue.Stop();
}

StorageRole InstalledFolderStorage::ClassifyRole_(
	const libtorrent::storage_params& parameters
) const {
	for (const libtorrent::file_index_t fileIndex : parameters.files.file_range()) {
		if (parameters.files.pad_file_at(fileIndex)) {
			continue;
		}

		if (!_locator->Find(parameters.files.file_path(fileIndex)).has_value()) {
			return StorageRole::FetchWrite;
		}
	}

	return StorageRole::SeedRead;
}

libtorrent::storage_holder InstalledFolderStorage::new_torrent(
	const libtorrent::storage_params& parameters,
	const std::shared_ptr<void>&
) {
	const StorageRole role = ClassifyRole_(parameters);

	const std::scoped_lock lock(_guard);

	const MountedTorrent mounted{parameters.files, parameters.path, role, true};

	for (std::size_t index = 0; index < _torrents.size(); ++index) {
		if (!_torrents[index].mounted) {
			_torrents[index] = mounted;
			return libtorrent::storage_holder(
				libtorrent::storage_index_t(static_cast<std::uint32_t>(index)),
				*this
			);
		}
	}

	_torrents.push_back(mounted);

	return libtorrent::storage_holder(
		libtorrent::storage_index_t(static_cast<std::uint32_t>(_torrents.size() - 1)),
		*this
	);
}

void InstalledFolderStorage::remove_torrent(const libtorrent::storage_index_t index) {
	_rangeIo.ReleaseHandles();

	const std::scoped_lock lock(_guard);

	const std::size_t slot = static_cast<std::uint32_t>(index);
	if (slot < _torrents.size()) {
		_torrents[slot].mounted = false;
	}
}

std::optional<InstalledFolderStorage::MountedTorrent> InstalledFolderStorage::Mounted_(
	const libtorrent::storage_index_t storage
) const {
	const std::scoped_lock lock(_guard);

	const std::size_t slot = static_cast<std::uint32_t>(storage);
	if (slot >= _torrents.size() || !_torrents[slot].mounted) {
		return std::nullopt;
	}

	return _torrents[slot];
}

ReadOutcome InstalledFolderStorage::ReadRange_(
	const libtorrent::storage_index_t storage,
	const libtorrent::piece_index_t piece,
	const std::int64_t offset,
	const std::int64_t length,
	char* target,
	libtorrent::file_index_t& failedFile
) const {
	const std::optional<MountedTorrent> mounted = Mounted_(storage);

	if (!mounted.has_value()) {
		return ReadOutcome::Fault;
	}

	return _rangeIo.Read(
		mounted->files,
		mounted->savePath,
		mounted->role,
		piece,
		offset,
		length,
		target,
		failedFile
	);
}

void InstalledFolderStorage::async_read(
	libtorrent::storage_index_t storage,
	const libtorrent::peer_request& request,
	std::function<void(libtorrent::disk_buffer_holder, const libtorrent::storage_error&)> handler,
	libtorrent::disk_job_flags_t
) {
	_queue.Enqueue(
		[this, storage, request, handler = std::move(handler)]() mutable {
			char* const buffer = new char[static_cast<std::size_t>(request.length)];

			libtorrent::file_index_t failedFile(0);

			const ReadOutcome outcome =
					ReadRange_(storage, request.piece, request.start, request.length, buffer, failedFile);

			if (outcome != ReadOutcome::Success) {
				delete[] buffer;

				if (outcome == ReadOutcome::Fault) {
					_faults->RecordReadFailure();
				}

				const libtorrent::storage_error failure = ChunkRangeIo::ReadError(outcome, failedFile);

				Complete_([handler, failure]() mutable {
						handler(libtorrent::disk_buffer_holder(), failure);
					}
				);

				return;
			}

			Complete_([this, handler, buffer]() mutable {
					handler(libtorrent::disk_buffer_holder(*this, buffer), libtorrent::storage_error());
				}
			);
		}
	);
}

bool InstalledFolderStorage::async_write(
	libtorrent::storage_index_t storage,
	const libtorrent::peer_request& request,
	const char* buffer,
	std::shared_ptr<libtorrent::disk_observer>,
	std::function<void(const libtorrent::storage_error&)> handler,
	libtorrent::disk_job_flags_t
) {
	std::vector<char> payload(buffer, buffer + request.length);

	const std::optional<MountedTorrent> target = Mounted_(storage);
	const bool tracked = target.has_value() && _locator->IsFetchStaging(target->savePath);

	if (tracked) {
		_backlog->Begin();
	}

	_queue.Enqueue(
		[this, storage, request, tracked, payload = std::move(payload), handler = std::move(handler)]() mutable {
			const std::optional<MountedTorrent> mounted = Mounted_(storage);

			const bool written = mounted.has_value()
			                     && _rangeIo.Write(
					                     mounted->files,
					                     mounted->savePath,
					                     mounted->role,
					                     request.piece,
					                     request.start,
					                     request.length,
					                     payload.data()
			                     );

			if (!written) {
				_faults->RecordWriteFailure();
			}

			if (tracked) {
				_backlog->Finish();
			}

			Complete_([handler, written]() mutable {
					handler(written ? libtorrent::storage_error() : ChunkRangeIo::IoFailure());
				}
			);
		}
	);

	return false;
}

void InstalledFolderStorage::async_hash(
	libtorrent::storage_index_t storage,
	libtorrent::piece_index_t piece,
	libtorrent::span<libtorrent::sha256_hash> blockHashes,
	libtorrent::disk_job_flags_t,
	std::function<void(libtorrent::piece_index_t, const libtorrent::sha1_hash&, const libtorrent::storage_error&)> handler
) {
	_queue.Enqueue([this, storage, piece, blockHashes, handler = std::move(handler)]() mutable {
			const std::optional<MountedTorrent> mounted = Mounted_(storage);

			if (!mounted.has_value()) {
				Complete_([handler, piece]() mutable {
						handler(piece, libtorrent::sha1_hash(), ChunkRangeIo::IoFailure());
					}
				);
				return;
			}

			const std::int64_t pieceSize = mounted->files.piece_size2(piece);

			std::vector<char> block(ChunkMerkleHasher::BLOCK_BYTES);
			libtorrent::hasher pieceHasher;

			std::int64_t offset = 0;
			int blockIndex = 0;

			while (offset < pieceSize) {
				const std::int64_t length = std::min<std::int64_t>(
					ChunkMerkleHasher::BLOCK_BYTES,
					pieceSize - offset
				);

				libtorrent::file_index_t failedFile(0);

				const ReadOutcome outcome =
						ReadRange_(storage, piece, offset, length, block.data(), failedFile);

				if (outcome != ReadOutcome::Success) {
					if (outcome == ReadOutcome::Fault) {
						_faults->RecordReadFailure();
					}

					const libtorrent::storage_error failure = ChunkRangeIo::ReadError(outcome, failedFile);

					Complete_([handler, piece, failure]() mutable {
							handler(piece, libtorrent::sha1_hash(), failure);
						}
					);
					return;
				}

				const libtorrent::span<const char> view(block.data(), length);
				pieceHasher.update(view);

				if (blockIndex < blockHashes.size()) {
					blockHashes[blockIndex] = libtorrent::hasher256(view).final();
				}

				offset += length;
				++blockIndex;
			}

			const libtorrent::sha1_hash digest = pieceHasher.final();

			Complete_([handler, piece, digest]() mutable {
					handler(piece, digest, libtorrent::storage_error());
				}
			);
		}
	);
}

void InstalledFolderStorage::async_hash2(
	libtorrent::storage_index_t storage,
	libtorrent::piece_index_t piece,
	int offset,
	libtorrent::disk_job_flags_t,
	std::function<void(libtorrent::piece_index_t, const libtorrent::sha256_hash&, const libtorrent::storage_error&)> handler
) {
	_queue.Enqueue([this, storage, piece, offset, handler = std::move(handler)]() mutable {
			const std::optional<MountedTorrent> mounted = Mounted_(storage);

			if (!mounted.has_value()) {
				Complete_([handler, piece]() mutable {
						handler(piece, libtorrent::sha256_hash(), ChunkRangeIo::IoFailure());
					}
				);
				return;
			}

			const std::int64_t pieceSize = mounted->files.piece_size2(piece);
			const std::int64_t available = pieceSize - offset;

			if (available <= 0) {
				Complete_([handler, piece]() mutable {
						handler(piece, libtorrent::sha256_hash(), ChunkRangeIo::IoFailure());
					}
				);
				return;
			}

			const std::int64_t length =
					std::min<std::int64_t>(ChunkMerkleHasher::BLOCK_BYTES, available);

			std::vector<char> block(static_cast<std::size_t>(length));

			libtorrent::file_index_t failedFile(0);

			const ReadOutcome outcome =
					ReadRange_(storage, piece, offset, length, block.data(), failedFile);

			if (outcome != ReadOutcome::Success) {
				if (outcome == ReadOutcome::Fault) {
					_faults->RecordReadFailure();
				}

				const libtorrent::storage_error failure = ChunkRangeIo::ReadError(outcome, failedFile);

				Complete_([handler, piece, failure]() mutable {
						handler(piece, libtorrent::sha256_hash(), failure);
					}
				);
				return;
			}

			const libtorrent::span<const char> view(
				block.data(),
				length
			);

			const libtorrent::sha256_hash digest = libtorrent::hasher256(view).final();

			Complete_([handler, piece, digest]() mutable {
					handler(piece, digest, libtorrent::storage_error());
				}
			);
		}
	);
}

void InstalledFolderStorage::async_move_storage(
	libtorrent::storage_index_t,
	const std::string path,
	libtorrent::move_flags_t,
	const std::function<void(libtorrent::status_t, const std::string&, const libtorrent::storage_error&)> handler
) {
	handler(libtorrent::status_t{}, path, ChunkRangeIo::IoFailure());
}

void InstalledFolderStorage::async_release_files(
	libtorrent::storage_index_t,
	const std::function<void()> handler
) {
	_rangeIo.ReleaseHandles();

	if (handler) {
		handler();
	}
}

void InstalledFolderStorage::async_check_files(
	const libtorrent::storage_index_t storage,
	const libtorrent::add_torrent_params* resumeData,
	libtorrent::aux::vector<std::string, libtorrent::file_index_t>,
	const std::function<void(libtorrent::status_t, const libtorrent::storage_error&)> handler
) {
	const std::optional<MountedTorrent> mounted = Mounted_(storage);

	if (!mounted.has_value()) {
		handler(libtorrent::status_t{}, libtorrent::storage_error());
		return;
	}

	if (mounted->role == StorageRole::SeedRead) {
		if (_attestations->Attests(mounted->files.name())
		    || ResumeAttestation::AttestsPieces(resumeData, mounted->files.num_pieces())) {
			handler(libtorrent::status_t{}, libtorrent::storage_error());
			return;
		}

		handler(libtorrent::disk_status::need_full_check, libtorrent::storage_error());
		return;
	}

	if (_locator->VerifyExisting()) {
		handler(libtorrent::disk_status::need_full_check, libtorrent::storage_error());
		return;
	}

	handler(libtorrent::status_t{}, libtorrent::storage_error());
}

void InstalledFolderStorage::async_stop_torrent(
	libtorrent::storage_index_t,
	const std::function<void()> handler
) {
	_rangeIo.ReleaseHandles();

	if (handler) {
		handler();
	}
}

void InstalledFolderStorage::async_rename_file(
	libtorrent::storage_index_t,
	const libtorrent::file_index_t index,
	const std::string name,
	const std::function<void(const std::string&, libtorrent::file_index_t, const libtorrent::storage_error&)> handler
) {
	handler(name, index, ChunkRangeIo::IoFailure());
}

void InstalledFolderStorage::async_delete_files(
	libtorrent::storage_index_t,
	libtorrent::remove_flags_t,
	const std::function<void(const libtorrent::storage_error&)> handler
) {
	handler(libtorrent::storage_error());
}

void InstalledFolderStorage::async_set_file_priority(
	libtorrent::storage_index_t,
	libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t> priorities,
	const std::function<void(const libtorrent::storage_error&, libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t>)> handler
) {
	handler(libtorrent::storage_error(), std::move(priorities));
}

void InstalledFolderStorage::async_clear_piece(
	libtorrent::storage_index_t,
	const libtorrent::piece_index_t index,
	const std::function<void(libtorrent::piece_index_t)> handler
) {
	handler(index);
}

void InstalledFolderStorage::update_stats_counters(libtorrent::counters&) const {}

std::vector<libtorrent::open_file_state> InstalledFolderStorage::get_status(
	libtorrent::storage_index_t
) const {
	return {};
}

void InstalledFolderStorage::abort(bool) {
	_queue.Stop();
}

void InstalledFolderStorage::Complete_(std::function<void()> completion) const {
	boost::asio::post(*_context, std::move(completion));
}

void InstalledFolderStorage::submit_jobs() {
	_queue.Submit();
}

void InstalledFolderStorage::settings_updated() {}

void InstalledFolderStorage::free_disk_buffer(char* buffer) {
	delete[] buffer;
}

void InstalledFolderStorage::free_multiple_buffers(const libtorrent::span<char*> buffers) {
	for (char* const buffer : buffers) {
		delete[] buffer;
	}
}
}
