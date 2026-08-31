#pragma once

#include "downloader/storage/ChunkLocator.h"
#include "downloader/storage/ChunkRangeIo.h"
#include "downloader/storage/SeedAttestations.h"
#include "downloader/storage/StorageBacklog.h"
#include "downloader/storage/StorageFaults.h"
#include "downloader/storage/StorageJobQueue.h"

#include <libtorrent/disk_buffer_holder.hpp>
#include <libtorrent/disk_interface.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/io_context.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace wgrd::downloader {
class InstalledFolderStorage final
		: public libtorrent::disk_interface,
		  public libtorrent::buffer_allocator_interface {
public:
	InstalledFolderStorage(
		const ChunkLocator& locator,
		libtorrent::io_context& context,
		StorageFaults& faults,
		const SeedAttestations& attestations,
		StorageBacklog& backlog
	);

	~InstalledFolderStorage() override;

	libtorrent::storage_holder new_torrent(
		const libtorrent::storage_params& parameters,
		const std::shared_ptr<void>& torrent
	) override;

	void remove_torrent(libtorrent::storage_index_t index) override;

	void async_read(
		libtorrent::storage_index_t storage,
		const libtorrent::peer_request& request,
		std::function<void(libtorrent::disk_buffer_holder, const libtorrent::storage_error&)> handler,
		libtorrent::disk_job_flags_t flags = {}
	) override;

	bool async_write(
		libtorrent::storage_index_t storage,
		const libtorrent::peer_request& request,
		const char* buffer,
		std::shared_ptr<libtorrent::disk_observer> observer,
		std::function<void(const libtorrent::storage_error&)> handler,
		libtorrent::disk_job_flags_t flags = {}
	) override;

	void async_hash(
		libtorrent::storage_index_t storage,
		libtorrent::piece_index_t piece,
		libtorrent::span<libtorrent::sha256_hash> blockHashes,
		libtorrent::disk_job_flags_t flags,
		std::function<void(libtorrent::piece_index_t, const libtorrent::sha1_hash&, const libtorrent::storage_error&)> handler
	) override;

	void async_hash2(
		libtorrent::storage_index_t storage,
		libtorrent::piece_index_t piece,
		int offset,
		libtorrent::disk_job_flags_t flags,
		std::function<void(libtorrent::piece_index_t, const libtorrent::sha256_hash&, const libtorrent::storage_error&)> handler
	) override;

	void async_move_storage(
		libtorrent::storage_index_t storage,
		std::string path,
		libtorrent::move_flags_t flags,
		std::function<void(libtorrent::status_t, const std::string&, const libtorrent::storage_error&)> handler
	) override;

	void async_release_files(
		libtorrent::storage_index_t storage,
		std::function<void()> handler = std::function<void()>()
	) override;

	void async_check_files(
		libtorrent::storage_index_t storage,
		const libtorrent::add_torrent_params* resumeData,
		libtorrent::aux::vector<std::string, libtorrent::file_index_t> links,
		std::function<void(libtorrent::status_t, const libtorrent::storage_error&)> handler
	) override;

	void async_stop_torrent(
		libtorrent::storage_index_t storage,
		std::function<void()> handler = std::function<void()>()
	) override;

	void async_rename_file(
		libtorrent::storage_index_t storage,
		libtorrent::file_index_t index,
		std::string name,
		std::function<void(const std::string&, libtorrent::file_index_t, const libtorrent::storage_error&)> handler
	) override;

	void async_delete_files(
		libtorrent::storage_index_t storage,
		libtorrent::remove_flags_t options,
		std::function<void(const libtorrent::storage_error&)> handler
	) override;

	void async_set_file_priority(
		libtorrent::storage_index_t storage,
		libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t> priorities,
		std::function<void(const libtorrent::storage_error&, libtorrent::aux::vector<libtorrent::download_priority_t, libtorrent::file_index_t>)> handler
	) override;

	void async_clear_piece(
		libtorrent::storage_index_t storage,
		libtorrent::piece_index_t index,
		std::function<void(libtorrent::piece_index_t)> handler
	) override;

	void update_stats_counters(libtorrent::counters& counters) const override;

	std::vector<libtorrent::open_file_state> get_status(libtorrent::storage_index_t) const override;

	void abort(bool wait) override;

	void submit_jobs() override;

	void settings_updated() override;

	void free_disk_buffer(char* buffer) override;

	void free_multiple_buffers(libtorrent::span<char*> buffers) override;

private:
	struct MountedTorrent {
		libtorrent::file_storage files;
		std::filesystem::path savePath;
		StorageRole role;
		bool mounted;
	};

	[[nodiscard]] StorageRole ClassifyRole_(const libtorrent::storage_params& parameters) const;

	[[nodiscard]] std::optional<MountedTorrent> Mounted_(libtorrent::storage_index_t storage) const;

	[[nodiscard]] ReadOutcome ReadRange_(
		libtorrent::storage_index_t storage,
		libtorrent::piece_index_t piece,
		std::int64_t offset,
		std::int64_t length,
		char* target,
		libtorrent::file_index_t& failedFile
	) const;

	void Complete_(std::function<void()> completion) const;

	const ChunkLocator* _locator;
	libtorrent::io_context* _context;
	StorageFaults* _faults;
	const SeedAttestations* _attestations;
	StorageBacklog* _backlog;
	ChunkRangeIo _rangeIo;
	mutable std::mutex _guard;
	StorageJobQueue _queue;
	std::vector<MountedTorrent> _torrents;
};
}
