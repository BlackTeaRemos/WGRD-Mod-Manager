#pragma once

#include <atomic>
#include <cstdint>

namespace wgrd::downloader {
class StorageFaults {
public:
	StorageFaults();

	StorageFaults(const StorageFaults&) = delete;

	StorageFaults& operator=(const StorageFaults&) = delete;

	void RecordReadFailure();

	void RecordWriteFailure();

	[[nodiscard]] std::uint64_t ReadFailures() const;

	[[nodiscard]] std::uint64_t WriteFailures() const;

	void Reset();

private:
	std::atomic<std::uint64_t> _readFailures;
	std::atomic<std::uint64_t> _writeFailures;
};
}
