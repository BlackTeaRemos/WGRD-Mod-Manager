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
        libtorrent::generic_category());
    return failure;
}

bool WriteFileRange(
    const std::filesystem::path& target,
    std::uint64_t offset,
    std::int64_t length,
    const char* source) {

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
    output.write(source, static_cast<std::streamsize>(length));

    return static_cast<bool>(output);
}

bool ReadFileRange(
    const std::filesystem::path& source,
    std::uint64_t offset,
    std::int64_t length,
    char* target) {

    std::ifstream input(source, std::ios::binary);
    if (!input) {
        return false;
    }

    input.seekg(static_cast<std::streamoff>(offset));
    input.read(target, static_cast<std::streamsize>(length));

    return input.gcount() == static_cast<std::streamsize>(length);
}

}

InstalledFolderStorage::InstalledFolderStorage(
    const ChunkLocator& locator,
    libtorrent::io_context& context)
    : _locator(&locator),
      _context(&context),
      _guard(),
      _torrents() {
}

InstalledFolderStorage::~InstalledFolderStorage() = default;

libtorrent::storage_holder InstalledFolderStorage::new_torrent(
    const libtorrent::storage_params& parameters,
    const std::shared_ptr<void>&) {

    bool writable = false;

    for (const libtorrent::file_index_t fileIndex : parameters.files.file_range()) {
        if (parameters.files.pad_file_at(fileIndex)) {
            continue;
        }

        if (!_locator->Find(parameters.files.file_path(fileIndex)).has_value()) {
            writable = true;
            break;
        }
    }

    const std::lock_guard<std::mutex> lock(_guard);

    const MountedTorrent mounted{parameters.files, parameters.path, writable, true};

    for (std::size_t index = 0; index < _torrents.size(); ++index) {
        if (!_torrents[index].mounted) {
            _torrents[index] = mounted;
            return libtorrent::storage_holder(
                libtorrent::storage_index_t(static_cast<std::uint32_t>(index)),
                *this);
        }
    }

    _torrents.push_back(mounted);

    return libtorrent::storage_holder(
        libtorrent::storage_index_t(static_cast<std::uint32_t>(_torrents.size() - 1)),
        *this);
}

void InstalledFolderStorage::remove_torrent(libtorrent::storage_index_t index) {
    const std::lock_guard<std::mutex> lock(_guard);

    const std::size_t slot = static_cast<std::size_t>(static_cast<std::uint32_t>(index));
    if (slot < _torrents.size()) {
        _torrents[slot].mounted = false;
    }
}

bool InstalledFolderStorage::ReadRange_(
    libtorrent::storage_index_t storage,
    libtorrent::piece_index_t piece,
    std::int64_t offset,
    std::int64_t length,
    char* target) const {

    libtorrent::file_storage files;
    std::filesystem::path savePath;
    bool writable = false;

    {
        const std::lock_guard<std::mutex> lock(_guard);

        const std::size_t slot = static_cast<std::size_t>(static_cast<std::uint32_t>(storage));
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

        if (writable && ReadFileRange(
                savePath / relative,
                static_cast<std::uint64_t>(slice.offset),
                slice.size,
                target + written)) {
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
                target + written)) {
            return false;
        }

        written += slice.size;
    }

    return written == length;
}

bool InstalledFolderStorage::WriteRange_(
    libtorrent::storage_index_t storage,
    libtorrent::piece_index_t piece,
    std::int64_t offset,
    std::int64_t length,
    const char* source) const {

    libtorrent::file_storage files;
    std::filesystem::path savePath;
    bool writable = false;

    {
        const std::lock_guard<std::mutex> lock(_guard);

        const std::size_t slot = static_cast<std::size_t>(static_cast<std::uint32_t>(storage));
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

        if (!WriteFileRange(
                savePath / files.file_path(slice.file_index),
                static_cast<std::uint64_t>(slice.offset),
                slice.size,
                source + consumed)) {
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
    libtorrent::disk_job_flags_t) {

    Enqueue_(
        [this, storage, request, handler = std::move(handler)]() mutable {
            char* const buffer = new char[static_cast<std::size_t>(request.length)];

            if (!ReadRange_(storage, request.piece, request.start, request.length, buffer)) {
                delete[] buffer;
                handler(libtorrent::disk_buffer_holder(), ReadFailure());
                return;
            }

            handler(libtorrent::disk_buffer_holder(*this, buffer), libtorrent::storage_error());
        });
}

bool InstalledFolderStorage::async_write(
    libtorrent::storage_index_t storage,
    const libtorrent::peer_request& request,
    const char* buffer,
    std::shared_ptr<libtorrent::disk_observer>,
    std::function<void(const libtorrent::storage_error&)> handler,
    libtorrent::disk_job_flags_t) {

    std::vector<char> payload(buffer, buffer + request.length);

    Enqueue_(
        [this, storage, request, payload = std::move(payload), handler = std::move(handler)]() mutable {
            if (!WriteRange_(storage, request.piece, request.start, request.length, payload.data())) {
                handler(ReadFailure());
                return;
            }

            handler(libtorrent::storage_error());
        });

    return false;
}

void InstalledFolderStorage::async_hash(
    libtorrent::storage_index_t storage,
    libtorrent::piece_index_t piece,
    libtorrent::span<libtorrent::sha256_hash> blockHashes,
    libtorrent::disk_job_flags_t,
    std::function<void(libtorrent::piece_index_t, const libtorrent::sha1_hash&, const libtorrent::storage_error&)> handler) {

    Enqueue_([this, storage, piece, blockHashes, handler = std::move(handler)]() mutable {
    libtorrent::file_storage files;

    {
        const std::lock_guard<std::mutex> lock(_guard);

        const std::size_t slot = static_cast<std::size_t>(static_cast<std::uint32_t>(storage));
        if (slot >= _torrents.size() || !_torrents[slot].mounted) {
            handler(piece, libtorrent::sha1_hash(), ReadFailure());
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
            static_cast<std::int64_t>(ChunkMerkleHasher::BLOCK_BYTES),
            pieceSize - offset);

        if (!ReadRange_(storage, piece, offset, length, block.data())) {
            handler(piece, libtorrent::sha1_hash(), ReadFailure());
            return;
        }

        const libtorrent::span<const char> view(block.data(), static_cast<std::ptrdiff_t>(length));
        pieceHasher.update(view);

        if (blockIndex < blockHashes.size()) {
            blockHashes[blockIndex] = libtorrent::hasher256(view).final();
        }

        offset += length;
        ++blockIndex;
    }

    handler(piece, pieceHasher.final(), libtorrent::storage_error());
    });
}

void InstalledFolderStorage::async_hash2(
    libtorrent::storage_index_t storage,
    libtorrent::piece_index_t piece,
    int offset,
    libtorrent::disk_job_flags_t,
    std::function<void(libtorrent::piece_index_t, const libtorrent::sha256_hash&, const libtorrent::storage_error&)> handler) {

    Enqueue_([this, storage, piece, offset, handler = std::move(handler)]() mutable {
    libtorrent::file_storage files;

    {
        const std::lock_guard<std::mutex> lock(_guard);

        const std::size_t slot = static_cast<std::size_t>(static_cast<std::uint32_t>(storage));
        if (slot >= _torrents.size() || !_torrents[slot].mounted) {
            handler(piece, libtorrent::sha256_hash(), ReadFailure());
            return;
        }

        files = _torrents[slot].files;
    }

    const std::int64_t pieceSize = files.piece_size2(piece);
    const std::int64_t available = pieceSize - offset;

    if (available <= 0) {
        handler(piece, libtorrent::sha256_hash(), ReadFailure());
        return;
    }

    const std::int64_t length =
        std::min<std::int64_t>(static_cast<std::int64_t>(ChunkMerkleHasher::BLOCK_BYTES), available);

    std::vector<char> block(static_cast<std::size_t>(length));

    if (!ReadRange_(storage, piece, offset, length, block.data())) {
        handler(piece, libtorrent::sha256_hash(), ReadFailure());
        return;
    }

    const libtorrent::span<const char> view(
        block.data(),
        static_cast<std::ptrdiff_t>(length));

    handler(piece, libtorrent::hasher256(view).final(), libtorrent::storage_error());
    });
}

void InstalledFolderStorage::async_move_storage(
    libtorrent::storage_index_t,
    std::string path,
    libtorrent::move_flags_t,
    std::function<void(libtorrent::status_t, const std::string&, const libtorrent::storage_error&)> handler) {

    handler(libtorrent::status_t{}, path, ReadFailure());
}

void InstalledFolderStorage::async_release_files(
    libtorrent::storage_index_t,
    std::function<void()> handler) {

    if (handler) {
        handler();
    }
}

void InstalledFolderStorage::async_check_files(
    libtorrent::storage_index_t storage,
    const libtorrent::add_torrent_params*,
    libtorrent::aux::vector<std::string, libtorrent::file_index_t>,
    std::function<void(libtorrent::status_t, const libtorrent::storage_error&)> handler) {

    bool writable = true;

    {
        const std::lock_guard<std::mutex> lock(_guard);

        const std::size_t slot = static_cast<std::size_t>(static_cast<std::uint32_t>(storage));
        if (slot < _torrents.size() && _torrents[slot].mounted) {
            writable = _torrents[slot].writable;
        }
    }

    if (writable) {
        handler(libtorrent::status_t{}, libtorrent::storage_error());
        return;
    }

    handler(libtorrent::disk_status::need_full_check, libtorrent::storage_error());
}

void InstalledFolderStorage::async_stop_torrent(
    libtorrent::storage_index_t,
    std::function<void()> handler) {

    if (handler) {
        handler();
    }
}

void InstalledFolderStorage::async_rename_file(
    libtorrent::storage_index_t,
    libtorrent::file_index_t index,
    std::string name,
    std::function<void(const std::string&, libtorrent::file_index_t, const libtorrent::storage_error&)> handler) {

    handler(name, index, ReadFailure());
}

void InstalledFolderStorage::async_delete_files(
    libtorrent::storage_index_t,
    libtorrent::remove_flags_t,
    std::function<void(const libtorrent::storage_error&)> handler) {

    handler(libtorrent::storage_error());
}

void InstalledFolderStorage::async_set_file_priority(
    libtorrent::storage_index_t,
    libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t> priorities,
    std::function<void(const libtorrent::storage_error&, libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t>)> handler) {

    handler(libtorrent::storage_error(), std::move(priorities));
}

void InstalledFolderStorage::async_clear_piece(
    libtorrent::storage_index_t,
    libtorrent::piece_index_t index,
    std::function<void(libtorrent::piece_index_t)> handler) {

    handler(index);
}

void InstalledFolderStorage::update_stats_counters(libtorrent::counters&) const {
}

std::vector<libtorrent::open_file_state> InstalledFolderStorage::get_status(
    libtorrent::storage_index_t) const {

    return {};
}

void InstalledFolderStorage::abort(bool) {
}

void InstalledFolderStorage::Enqueue_(std::function<void()> job) {
    const std::lock_guard<std::mutex> lock(_jobGuard);
    _jobs.push_back(std::move(job));
}

void InstalledFolderStorage::submit_jobs() {
    std::deque<std::function<void()>> pending;

    {
        const std::lock_guard<std::mutex> lock(_jobGuard);
        pending.swap(_jobs);
    }

    for (std::function<void()>& job : pending) {
        boost::asio::post(*_context, std::move(job));
    }
}

void InstalledFolderStorage::settings_updated() {
}

void InstalledFolderStorage::free_disk_buffer(char* buffer) {
    delete[] buffer;
}

void InstalledFolderStorage::free_multiple_buffers(libtorrent::span<char*> buffers) {
    for (char* const buffer : buffers) {
        delete[] buffer;
    }
}

}
