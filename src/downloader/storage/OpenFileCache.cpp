#include "downloader/storage/OpenFileCache.h"

#include <windows.h>

#include <system_error>
#include <utility>

namespace wgrd::downloader {
namespace {
	constexpr DWORD SHARE_MODE = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
}

OpenFileCache::OpenFileCache()
	: _guard()
	, _entries() {}

OpenFileCache::~OpenFileCache() {
	Clear();
}

void OpenFileCache::Close_(Entry& entry) {
	if (entry.handle != INVALID_HANDLE_VALUE && entry.handle != nullptr) {
		CloseHandle(entry.handle);
	}

	entry.handle = INVALID_HANDLE_VALUE;
}

OpenFileCache::Entry* OpenFileCache::Acquire_(
	const std::filesystem::path& path,
	const bool needWrite
) const {
	for (auto candidate = _entries.begin(); candidate != _entries.end(); ++candidate) {
		if (candidate->path != path) {
			continue;
		}

		if (needWrite && !candidate->writable) {
			Close_(*candidate);
			_entries.erase(candidate);
			break;
		}

		_entries.splice(_entries.begin(), _entries, candidate);

		return &_entries.front();
	}

	if (needWrite) {
		std::error_code failure;
		std::filesystem::create_directories(path.parent_path(), failure);
	}

	const DWORD access = needWrite ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
	const DWORD disposition = needWrite ? OPEN_ALWAYS : OPEN_EXISTING;

	void* const handle = CreateFileW(
		path.c_str(),
		access,
		SHARE_MODE,
		nullptr,
		disposition,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (handle == INVALID_HANDLE_VALUE) {
		return nullptr;
	}

	_entries.push_front(Entry{path, needWrite, handle});

	while (_entries.size() > MAXIMUM_HANDLES) {
		Close_(_entries.back());
		_entries.pop_back();
	}

	return &_entries.front();
}

bool OpenFileCache::Read(
	const std::filesystem::path& source,
	const std::uint64_t offset,
	const std::int64_t length,
	char* target
) const {
	const std::scoped_lock lock(_guard);

	const Entry* const entry = Acquire_(source, false);
	if (entry == nullptr) {
		return false;
	}

	std::int64_t moved = 0;

	while (moved < length) {
		OVERLAPPED position{};
		const std::uint64_t at = offset + static_cast<std::uint64_t>(moved);
		position.Offset = static_cast<DWORD>(at & 0xFFFFFFFFu);
		position.OffsetHigh = static_cast<DWORD>(at >> 32);

		DWORD taken = 0;
		const BOOL done = ReadFile(
			entry->handle,
			target + moved,
			static_cast<DWORD>(length - moved),
			&taken,
			&position
		);

		if (done == FALSE || taken == 0) {
			return false;
		}

		moved += static_cast<std::int64_t>(taken);
	}

	return true;
}

bool OpenFileCache::Write(
	const std::filesystem::path& target,
	const std::uint64_t offset,
	const std::int64_t length,
	const char* source
) const {
	const std::scoped_lock lock(_guard);

	const Entry* const entry = Acquire_(target, true);
	if (entry == nullptr) {
		return false;
	}

	std::int64_t moved = 0;

	while (moved < length) {
		OVERLAPPED position{};
		const std::uint64_t at = offset + static_cast<std::uint64_t>(moved);
		position.Offset = static_cast<DWORD>(at & 0xFFFFFFFFu);
		position.OffsetHigh = static_cast<DWORD>(at >> 32);

		DWORD taken = 0;
		const BOOL done = WriteFile(
			entry->handle,
			source + moved,
			static_cast<DWORD>(length - moved),
			&taken,
			&position
		);

		if (done == FALSE || taken == 0) {
			return false;
		}

		moved += static_cast<std::int64_t>(taken);
	}

	return true;
}

void OpenFileCache::Forget(const std::filesystem::path& path) const {
	const std::scoped_lock lock(_guard);

	for (auto candidate = _entries.begin(); candidate != _entries.end();) {
		if (candidate->path == path) {
			Close_(*candidate);
			candidate = _entries.erase(candidate);
			continue;
		}

		++candidate;
	}
}

void OpenFileCache::Clear() const {
	const std::scoped_lock lock(_guard);

	for (Entry& entry : _entries) {
		Close_(entry);
	}

	_entries.clear();
}
}
