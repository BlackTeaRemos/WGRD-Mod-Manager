#include "downloader/transfer/FetchState.h"

#include <algorithm>
#include <utility>

namespace wgrd::downloader {
FetchState::FetchState()
	: _guard()
	, _fetch()
	, _fetching()
	, _wanted()
	, _prioritised(false)
	, _settledPolls(0)
	, _removing()
	, _clearPending(false) {}

bool FetchState::Reserve(
	std::string identifier,
	std::filesystem::path stagingFolder,
	std::set<std::string> wantedFiles
) {
	const std::scoped_lock lock(_guard);

	if (_fetch.Busy()) {
		return false;
	}

	_fetch = domain::FetchStatus{};
	_fetch.phase = domain::FetchPhase::Metadata;
	_fetch.identifier = std::move(identifier);
	_fetch.stagingFolder = std::move(stagingFolder);

	_wanted = std::move(wantedFiles);
	_prioritised = false;
	_settledPolls = 0;

	return true;
}

void FetchState::Adopt(const libtorrent::torrent_handle& handle) {
	const std::scoped_lock lock(_guard);
	_fetching = handle;
}

void FetchState::Release() {
	const std::scoped_lock lock(_guard);

	_fetching.reset();
	_wanted.clear();
	_prioritised = false;
	_settledPolls = 0;
	_fetch.phase = domain::FetchPhase::Idle;
}

FetchRetirement FetchState::Retire() {
	const std::scoped_lock lock(_guard);

	FetchRetirement retirement{std::nullopt, false};

	if (_fetching.has_value() && _fetching->is_valid()) {
		_removing.push_back(*_fetching);
		retirement.toRemove = *_fetching;
	}

	if (_removing.empty()) {
		retirement.clearNow = true;
	} else {
		_clearPending = true;
	}

	_fetching.reset();
	_wanted.clear();
	_prioritised = false;
	_settledPolls = 0;

	if (_fetch.Busy()) {
		_fetch.phase = domain::FetchPhase::Idle;
	}

	return retirement;
}

bool FetchState::ConfirmRemoval(const libtorrent::torrent_handle& handle) {
	const std::scoped_lock lock(_guard);

	const auto match = std::ranges::find(_removing, handle);
	if (match == _removing.end()) {
		return false;
	}

	_removing.erase(match);

	if (!_removing.empty() || !_clearPending) {
		return false;
	}

	_clearPending = false;

	return true;
}

bool FetchState::AbandonPendingClear() {
	const std::scoped_lock lock(_guard);

	if (!_clearPending) {
		return false;
	}

	_clearPending = false;

	return true;
}

std::optional<libtorrent::torrent_handle> FetchState::Active() const {
	const std::scoped_lock lock(_guard);
	return _fetching;
}

domain::FetchStatus FetchState::Snapshot() const {
	const std::scoped_lock lock(_guard);
	return _fetch;
}

std::set<std::string> FetchState::WantedFiles() const {
	const std::scoped_lock lock(_guard);
	return _wanted;
}

bool FetchState::Prioritised() const {
	const std::scoped_lock lock(_guard);
	return _prioritised;
}

void FetchState::MarkPrioritised(const std::uint64_t wantedBytes) {
	const std::scoped_lock lock(_guard);

	_fetch.wantedBytes = wantedBytes;

	if (_fetch.phase == domain::FetchPhase::Metadata) {
		_fetch.phase = domain::FetchPhase::Downloading;
	}

	_prioritised = true;
}

void FetchState::CountHashFailure(const std::string_view failure) {
	const std::scoped_lock lock(_guard);

	if (!_fetch.Busy()) {
		return;
	}

	++_fetch.hashFailures;
	_fetch.lastFailure = std::string(failure);
}

void FetchState::CountBannedPeer(const std::string_view failure) {
	const std::scoped_lock lock(_guard);

	if (!_fetch.Busy()) {
		return;
	}

	++_fetch.bannedPeers;
	_fetch.lastFailure = std::string(failure);
}

void FetchState::Fail(const std::string_view failure) {
	const std::scoped_lock lock(_guard);

	if (!_fetch.Busy()) {
		return;
	}

	_fetch.phase = domain::FetchPhase::Failed;
	_fetch.lastFailure = std::string(failure);
}

void FetchState::Update(
	const std::uint32_t peers,
	const std::uint64_t fetchedBytes,
	const std::uint64_t inFlightBytes,
	const bool finished,
	const bool writesSettled
) {
	const std::scoped_lock lock(_guard);

	if (!_fetch.Busy()) {
		return;
	}

	_fetch.peers = peers;
	_fetch.fetchedBytes = fetchedBytes;
	_fetch.inFlightBytes = inFlightBytes;

	const bool wantedComplete = _fetch.wantedBytes > 0
	                            && _fetch.fetchedBytes >= _fetch.wantedBytes
	                            && _fetch.inFlightBytes == 0;

	if (!_prioritised || !writesSettled || !(finished || wantedComplete)) {
		_settledPolls = 0;
		return;
	}

	if (_settledPolls < COMPLETE_CONFIRMATIONS) {
		++_settledPolls;
		return;
	}

	_fetch.phase = domain::FetchPhase::Complete;
}
}
