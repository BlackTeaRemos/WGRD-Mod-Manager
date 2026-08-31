#include "downloader/storage/StorageBacklog.h"

#include <thread>

namespace wgrd::downloader {
StorageBacklog::StorageBacklog()
	: _pending(0) {}

void StorageBacklog::Begin() {
	_pending.fetch_add(1, std::memory_order_acq_rel);
}

void StorageBacklog::Finish() {
	_pending.fetch_sub(1, std::memory_order_acq_rel);
}

std::uint64_t StorageBacklog::Pending() const {
	return _pending.load(std::memory_order_acquire);
}

bool StorageBacklog::AwaitDrain(const std::chrono::milliseconds timeout) const {
	const auto deadline = std::chrono::steady_clock::now() + timeout;

	while (Pending() > 0) {
		if (std::chrono::steady_clock::now() >= deadline) {
			return false;
		}

		std::this_thread::sleep_for(DRAIN_POLL_INTERVAL);
	}

	return true;
}
}
