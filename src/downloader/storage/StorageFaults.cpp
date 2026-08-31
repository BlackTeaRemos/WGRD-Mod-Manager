#include "downloader/storage/StorageFaults.h"

namespace wgrd::downloader {
StorageFaults::StorageFaults()
	: _readFailures(0)
	, _writeFailures(0) {}

void StorageFaults::RecordReadFailure() {
	_readFailures.fetch_add(1, std::memory_order_relaxed);
}

void StorageFaults::RecordWriteFailure() {
	_writeFailures.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t StorageFaults::ReadFailures() const {
	return _readFailures.load(std::memory_order_relaxed);
}

std::uint64_t StorageFaults::WriteFailures() const {
	return _writeFailures.load(std::memory_order_relaxed);
}

void StorageFaults::Reset() {
	_readFailures.store(0, std::memory_order_relaxed);
	_writeFailures.store(0, std::memory_order_relaxed);
}
}
