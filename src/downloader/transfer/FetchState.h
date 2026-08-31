#pragma once

#include "domain/interfaces/content/IChunkFetcher.h"

#include <libtorrent/torrent_handle.hpp>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::downloader {
struct FetchRetirement {
	std::optional<libtorrent::torrent_handle> toRemove;
	bool clearNow;
};

class FetchState {
public:
	static constexpr int COMPLETE_CONFIRMATIONS = 3;

	explicit FetchState();

	FetchState(const FetchState&) = delete;

	FetchState& operator=(const FetchState&) = delete;

	[[nodiscard]] bool Reserve(
		std::string identifier,
		std::filesystem::path stagingFolder,
		std::set<std::string> wantedFiles
	);

	void Adopt(const libtorrent::torrent_handle& handle);

	void Release();

	[[nodiscard]] FetchRetirement Retire();

	[[nodiscard]] bool ConfirmRemoval(const libtorrent::torrent_handle& handle);

	[[nodiscard]] bool AbandonPendingClear();

	[[nodiscard]] std::optional<libtorrent::torrent_handle> Active() const;

	[[nodiscard]] domain::FetchStatus Snapshot() const;

	[[nodiscard]] std::set<std::string> WantedFiles() const;

	[[nodiscard]] bool Prioritised() const;

	void MarkPrioritised(std::uint64_t wantedBytes);

	void CountHashFailure(std::string_view failure);

	void CountBannedPeer(std::string_view failure);

	void Fail(std::string_view failure);

	void Update(
		std::uint32_t peers,
		std::uint64_t fetchedBytes,
		std::uint64_t inFlightBytes,
		bool finished
	);

private:
	mutable std::mutex _guard;
	domain::FetchStatus _fetch;
	std::optional<libtorrent::torrent_handle> _fetching;
	std::set<std::string> _wanted;
	bool _prioritised;
	int _settledPolls;
	std::vector<libtorrent::torrent_handle> _removing;
	bool _clearPending;
};
}
