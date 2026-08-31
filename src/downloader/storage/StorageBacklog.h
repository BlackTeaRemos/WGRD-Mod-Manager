#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace wgrd::downloader {
class StorageBacklog {
public:
	static constexpr std::chrono::milliseconds DRAIN_POLL_INTERVAL{5};

	StorageBacklog();

	StorageBacklog(const StorageBacklog&) = delete;

	StorageBacklog& operator=(const StorageBacklog&) = delete;

	void Begin();

	void Finish();

	[[nodiscard]] std::uint64_t Pending() const;

	[[nodiscard]] bool AwaitDrain(std::chrono::milliseconds timeout) const;

private:
	std::atomic<std::uint64_t> _pending;
};
}
