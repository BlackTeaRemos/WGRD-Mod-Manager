#include "downloader/storage/InstalledFolderStorage.h"

#include "downloader/torrent/build/ChunkMerkleHasher.h"

#include <libtorrent/error_code.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/peer_request.hpp>
#include <libtorrent/storage_defs.hpp>

#include <boost/asio/post.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace wgrd::downloader {
namespace {
	libtorrent::storage_error ReadFailure() {
		libtorrent::storage_error failure;
		failure.ec = libtorrent::error_code(
			boost::system::errc::io_error,
			libtorrent::generic_category()
		);
		return failure;
	}

	bool WriteFileRange(
		const std::filesystem::path& target,
		const std::uint64_t offset,
		const std::int64_t length,
		const char* source
	) {
		std::error_code failure;
		std::filesystem::create_directories(target.parent_path(), failure);

		if (!std::filesystem::exists(target, failure)) {
			std::ofstream created(target, std::ios::binary);
			if (!created) {
				return false;
			}
		}

		std::fstream output(target, std::ios::binary | std::ios::in | std::ios::out);
		if (!output) {
			return false;
		}

		output.seekp(static_cast<std::streamoff>(offset));
		output.write(source, length);

		return static_cast<bool>(output);
	}

	bool ReadFileRange(
		const std::filesystem::path& source,
		const std::uint64_t offset,
		const std::int64_t length,
		char* target
	) {
		std::ifstream input(source, std::ios::binary);
		if (!input) {
			return false;
		}

		input.seekg(static_cast<std::streamoff>(offset));
		input.read(target, length);

		return input.gcount() == length;
	}
}

InstalledFolderStorage::InstalledFolderStorage(
	const ChunkLocator& locator,
	libtorrent::io_context& context,
	StorageFaults& faults
)
	: _locator(&locator)
	, _context(&context)
	, _faults(&faults)
	, _guard()
	, _jobGuard()
	, _jobSignal()
	, _jobs()
	, _ready()
	, _stopping(false)
	, _worker()
	, _torrents() {
	_worker = std::thread([this]() {
			RunWorker_();
		}
	);
}

InstalledFolderStorage::~InstalledFolderStorage() {
	StopWorker_();
}

libtorrent::storage_holder InstalledFolderStorage::new_torrent(
	const libtorrent::storage_params& parameters,
	const std::shared_ptr<void>&
) {
	bool writable = false;

	for (const libtorrent::file_index_t fileIndex : parameters.files.file_range()) {
		if (parameters.files.pad_file_at(fileIndex)) {
			continue;
		}

		if (_locator->HasDestination(parameters.files.file_path(fileIndex))) {
			writable = true;
			break;
		}
	}

	if (!writable) {
		for (const libtorrent::file_index_t fileIndex : parameters.files.file_range()) {
			if (parameters.files.pad_file_at(fileIndex)) {
				continue;
			}

			if (!_locator->Find(parameters.files.file_path(fileIndex)).has_value()) {
				writable = true;
				break;
			}
		}
	}

	const std::scoped_lock lock(_guard);

	const MountedTorrent mounted{parameters.files, parameters.path, writable, true};

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
	const std::scoped_lock lock(_guard);

	const std::size_t slot = static_cast<std::uint32_t>(index);
	if (slot < _torrents.size()) {
		_torrents[slot].mounted = false;
	}
}

bool InstalledFolderStorage::ReadRange_(
	libtorrent::storage_index_t storage,
	libtorrent::piece_index_t piece,
	std::int64_t offset,
	std::int64_t length,
	char* target
) const {
	libtorrent::file_storage files;
	std::filesystem::path savePath;
	bool writable = false;

	{
		const std::scoped_lock lock(_guard);

		const std::size_t slot = static_cast<std::uint32_t>(storage);
		if (slot >= _torrents.size() || !_torrents[slot].mounted) {
			return false;
		}

		files = _torrents[slot].files;
		savePath = _torrents[slot].savePath;
		writable = _torrents[slot].writable;
	}

	const std::vector<libtorrent::file_slice> slices = files.map_block(piece, offset, length);

	std::int64_t written = 0;

	for (const libtorrent::file_slice& slice : slices) {
		if (files.pad_file_at(slice.file_index)) {
			std::fill_n(target + written, slice.size, char{0});
			written += slice.size;
			continue;
		}

		const std::string relative = files.file_path(slice.file_index);

		if (writable) {
			const std::vector<ChunkLocation> destinations = _locator->FindDestinations(relative);

			if (destinations.empty()) {
				if (!ReadFileRange(
					savePath / relative,
					static_cast<std::uint64_t>(slice.offset),
					slice.size,
					target + written
				)) {
					return false;
				}

				written += slice.size;
				continue;
			}

			const ChunkLocation& destination = destinations.front();

			if (slice.offset + slice.size > static_cast<std::int64_t>(destination.length)) {
				return false;
			}

			if (!ReadFileRange(
				destination.file,
				destination.offset + static_cast<std::uint64_t>(slice.offset),
				slice.size,
				target + written
			)) {
				return false;
			}

			written += slice.size;
			continue;
		}

		const auto location = _locator->Find(relative);
		if (!location.has_value()) {
			return false;
		}

		if (slice.offset + slice.size > static_cast<std::int64_t>(location->length)) {
			return false;
		}

		if (!ReadFileRange(
			location->file,
			location->offset + static_cast<std::uint64_t>(slice.offset),
			slice.size,
			target + written
		)) {
			return false;
		}

		written += slice.size;
	}

	return written == length;
}

bool InstalledFolderStorage::WriteRange_(
	const libtorrent::storage_index_t storage,
	const libtorrent::piece_index_t piece,
	const std::int64_t offset,
	const std::int64_t length,
	const char* source
) const {
	libtorrent::file_storage files;
	std::filesystem::path savePath;
	bool writable = false;

	{
		const std::scoped_lock lock(_guard);

		const std::size_t slot = static_cast<std::uint32_t>(storage);
		if (slot >= _torrents.size() || !_torrents[slot].mounted) {
			return false;
		}

		files = _torrents[slot].files;
		savePath = _torrents[slot].savePath;
		writable = _torrents[slot].writable;
	}

	if (!writable) {
		return false;
	}

	const std::vector<libtorrent::file_slice> slices = files.map_block(piece, offset, length);

	std::int64_t consumed = 0;

	for (const libtorrent::file_slice& slice : slices) {
		if (files.pad_file_at(slice.file_index)) {
			consumed += slice.size;
			continue;
		}

		const std::string relative = files.file_path(slice.file_index);

		const std::vector<ChunkLocation> locations = _locator->FindDestinations(relative);

		if (!locations.empty()) {
			for (const ChunkLocation& location : locations) {
				if (slice.offset + slice.size > static_cast<std::int64_t>(location.length)) {
					return false;
				}

				if (!WriteFileRange(
					location.file,
					location.offset + static_cast<std::uint64_t>(slice.offset),
					slice.size,
					source + consumed
				)) {
					return false;
				}
			}

			consumed += slice.size;
			continue;
		}

		if (!WriteFileRange(
			savePath / relative,
			static_cast<std::uint64_t>(slice.offset),
			slice.size,
			source + consumed
		)) {
			return false;
		}

		consumed += slice.size;
	}

	return consumed == length;
}

void InstalledFolderStorage::async_read(
	libtorrent::storage_index_t storage,
	const libtorrent::peer_request& request,
	std::function<void(libtorrent::disk_buffer_holder, const libtorrent::storage_error&)> handler,
	libtorrent::disk_job_flags_t
) {
	Enqueue_(
		[this, storage, request, handler = std::move(handler)]() mutable {
			char* const buffer = new char[static_cast<std::size_t>(request.length)];

			if (!ReadRange_(storage, request.piece, request.start, request.length, buffer)) {
				delete[] buffer;

				_faults->RecordReadFailure();

				Complete_([handler]() mutable {
						handler(libtorrent::disk_buffer_holder(), ReadFailure());
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

	Enqueue_(
		[this, storage, request, payload = std::move(payload), handler = std::move(handler)]() mutable {
			const bool written =
					WriteRange_(storage, request.piece, request.start, request.length, payload.data());

			if (!written) {
				_faults->RecordWriteFailure();
			}

			Complete_([handler, written]() mutable {
					handler(written ? libtorrent::storage_error() : ReadFailure());
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
	Enqueue_([this, storage, piece, blockHashes, handler = std::move(handler)]() mutable {
			libtorrent::file_storage files;

			{
				const std::scoped_lock lock(_guard);

				const std::size_t slot = static_cast<std::uint32_t>(storage);
				if (slot >= _torrents.size() || !_torrents[slot].mounted) {
					Complete_([handler, piece]() mutable {
							handler(piece, libtorrent::sha1_hash(), ReadFailure());
						}
					);
					return;
				}

				files = _torrents[slot].files;
			}

			const std::int64_t pieceSize = files.piece_size2(piece);

			std::vector<char> block(ChunkMerkleHasher::BLOCK_BYTES);
			libtorrent::hasher pieceHasher;

			std::int64_t offset = 0;
			int blockIndex = 0;

			while (offset < pieceSize) {
				const std::int64_t length = std::min<std::int64_t>(
					ChunkMerkleHasher::BLOCK_BYTES,
					pieceSize - offset
				);

				if (!ReadRange_(storage, piece, offset, length, block.data())) {
					_faults->RecordReadFailure();

					Complete_([handler, piece]() mutable {
							handler(piece, libtorrent::sha1_hash(), ReadFailure());
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
	Enqueue_([this, storage, piece, offset, handler = std::move(handler)]() mutable {
			libtorrent::file_storage files;

			{
				const std::scoped_lock lock(_guard);

				const std::size_t slot = static_cast<std::uint32_t>(storage);
				if (slot >= _torrents.size() || !_torrents[slot].mounted) {
					Complete_([handler, piece]() mutable {
							handler(piece, libtorrent::sha256_hash(), ReadFailure());
						}
					);
					return;
				}

				files = _torrents[slot].files;
			}

			const std::int64_t pieceSize = files.piece_size2(piece);
			const std::int64_t available = pieceSize - offset;

			if (available <= 0) {
				Complete_([handler, piece]() mutable {
						handler(piece, libtorrent::sha256_hash(), ReadFailure());
					}
				);
				return;
			}

			const std::int64_t length =
					std::min<std::int64_t>(ChunkMerkleHasher::BLOCK_BYTES, available);

			std::vector<char> block(static_cast<std::size_t>(length));

			if (!ReadRange_(storage, piece, offset, length, block.data())) {
				_faults->RecordReadFailure();

				Complete_([handler, piece]() mutable {
						handler(piece, libtorrent::sha256_hash(), ReadFailure());
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
	handler(libtorrent::status_t{}, path, ReadFailure());
}

void InstalledFolderStorage::async_release_files(
	libtorrent::storage_index_t,
	const std::function<void()> handler
) {
	if (handler) {
		handler();
	}
}

void InstalledFolderStorage::async_check_files(
	const libtorrent::storage_index_t storage,
	const libtorrent::add_torrent_params*,
	libtorrent::aux::vector<std::string, libtorrent::file_index_t>,
	const std::function<void(libtorrent::status_t, const libtorrent::storage_error&)> handler
) {
	bool writable = true;

	{
		const std::scoped_lock lock(_guard);

		const std::size_t slot = static_cast<std::uint32_t>(storage);
		if (slot < _torrents.size() && _torrents[slot].mounted) {
			writable = _torrents[slot].writable;
		}
	}

	if (writable && !_locator->VerifyExisting()) {
		handler(libtorrent::status_t{}, libtorrent::storage_error());
		return;
	}

	handler(libtorrent::disk_status::need_full_check, libtorrent::storage_error());
}

void InstalledFolderStorage::async_stop_torrent(
	libtorrent::storage_index_t,
	const std::function<void()> handler
) {
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
	handler(name, index, ReadFailure());
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
	StopWorker_();
}

void InstalledFolderStorage::Enqueue_(std::function<void()> job) {
	const std::scoped_lock lock(_jobGuard);
	_jobs.push_back(std::move(job));
}

void InstalledFolderStorage::Complete_(std::function<void()> completion) const {
	boost::asio::post(*_context, std::move(completion));
}

void InstalledFolderStorage::submit_jobs() {
	{
		const std::scoped_lock lock(_jobGuard);

		if (_jobs.empty()) {
			return;
		}

		while (!_jobs.empty()) {
			_ready.push_back(std::move(_jobs.front()));
			_jobs.pop_front();
		}
	}

	_jobSignal.notify_one();
}

void InstalledFolderStorage::RunWorker_() {
	while (true) {
		std::function<void()> job;

		{
			std::unique_lock<std::mutex> lock(_jobGuard);

			_jobSignal.wait(lock, [this]() {
				                return _stopping || !_ready.empty();
			                }
			);

			if (_stopping && _ready.empty()) {
				return;
			}

			job = std::move(_ready.front());
			_ready.pop_front();
		}

		job();
	}
}

void InstalledFolderStorage::StopWorker_() {
	{
		const std::scoped_lock lock(_jobGuard);
		_stopping = true;
	}

	_jobSignal.notify_all();

	if (_worker.joinable()) {
		_worker.join();
	}
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
