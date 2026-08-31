#include "downloader/transfer/FetchRefresher.h"

#include "domain/types/content/ChunkFileNaming.h"

#include <libtorrent/download_priority.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace wgrd::downloader {
void FetchRefresher::ApplyPriorities_(FetchState& fetchState, libtorrent::torrent_handle& handle) {
	if (fetchState.Prioritised() || !handle.is_valid()) {
		return;
	}

	const std::shared_ptr<const libtorrent::torrent_info> info = handle.torrent_file();
	if (info == nullptr) {
		return;
	}

	const std::set<std::string> wantedFiles = fetchState.WantedFiles();

	const libtorrent::file_storage& files = info->layout();

	std::vector<libtorrent::download_priority_t> priorities;
	priorities.reserve(static_cast<std::size_t>(info->num_files()));

	std::uint64_t wantedBytes = 0;

	for (const libtorrent::file_index_t index : files.file_range()) {
		if (files.pad_file_at(index)) {
			priorities.push_back(libtorrent::dont_download);
			continue;
		}

		const std::string leaf(domain::ChunkFileNaming::LeafOf(files.file_path(index)));

		const bool wanted = wantedFiles.contains(leaf);

		priorities.push_back(wanted ? libtorrent::default_priority : libtorrent::dont_download);

		if (wanted) {
			wantedBytes += static_cast<std::uint64_t>(files.file_size(index));
		}
	}

	handle.prioritize_files(priorities);

	fetchState.MarkPrioritised(wantedBytes);
}

std::optional<FetchRates> FetchRefresher::Refresh(FetchState& fetchState, const bool writesSettled) {
	std::optional<libtorrent::torrent_handle> active = fetchState.Active();

	if (!active.has_value() || !active->is_valid()) {
		return std::nullopt;
	}

	ApplyPriorities_(fetchState, *active);

	const libtorrent::torrent_status status = active->status();

	const std::int64_t received = status.total_payload_download;
	const std::int64_t verified = status.total_wanted_done;

	const std::uint64_t inFlightBytes = received > verified
	                                    ? static_cast<std::uint64_t>(received - verified)
	                                    : 0;

	fetchState.Update(
		static_cast<std::uint32_t>(status.num_peers),
		static_cast<std::uint64_t>(status.total_wanted_done),
		inFlightBytes,
		status.is_finished,
		writesSettled
	);

	return FetchRates{status.download_payload_rate, status.upload_payload_rate};
}
}
