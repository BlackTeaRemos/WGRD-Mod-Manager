#include "downloader/announce/OutstandingWantTracker.h"

namespace wgrd::downloader {
OutstandingWantTracker::OutstandingWantTracker()
	: _pending() {}

bool OutstandingWantTracker::Track(const std::string& identifier, const Clock::time_point now) {
	const auto known = _pending.find(identifier);
	if (known != _pending.end()) {
		known->second = now;
		return true;
	}

	if (_pending.size() >= MAXIMUM_OUTSTANDING) {
		return false;
	}

	_pending.emplace(identifier, now);

	return true;
}

bool OutstandingWantTracker::Redeem(const std::string& identifier) {
	return _pending.erase(identifier) > 0;
}

void OutstandingWantTracker::Prune(const Clock::time_point now) {
	std::erase_if(_pending, [now](const auto& entry) {
		return now - entry.second >= WANT_EXPIRY;
	});
}

std::size_t OutstandingWantTracker::Count() const {
	return _pending.size();
}
}
