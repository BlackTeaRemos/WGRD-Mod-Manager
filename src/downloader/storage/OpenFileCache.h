#pragma once

#include <cstdint>
#include <filesystem>
#include <list>
#include <mutex>

namespace wgrd::downloader {
class OpenFileCache {
public:
	static constexpr std::size_t MAXIMUM_HANDLES = 8;

	OpenFileCache();

	OpenFileCache(const OpenFileCache&) = delete;

	OpenFileCache& operator=(const OpenFileCache&) = delete;

	~OpenFileCache();

	[[nodiscard]] bool Read(
		const std::filesystem::path& source,
		std::uint64_t offset,
		std::int64_t length,
		char* target
	) const;

	[[nodiscard]] bool Write(
		const std::filesystem::path& target,
		std::uint64_t offset,
		std::int64_t length,
		const char* source
	) const;

	void Forget(const std::filesystem::path& path) const;

	void Clear() const;

private:
	struct Entry {
		std::filesystem::path path;
		bool writable;
		void* handle;
	};

	[[nodiscard]] Entry* Acquire_(const std::filesystem::path& path, bool needWrite) const;

	static void Close_(Entry& entry);

	mutable std::mutex _guard;
	mutable std::list<Entry> _entries;
};
}
