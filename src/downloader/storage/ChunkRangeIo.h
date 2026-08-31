#pragma once

#include "downloader/storage/ChunkLocator.h"

#include <libtorrent/file_storage.hpp>
#include <libtorrent/storage_defs.hpp>

#include <cstdint>
#include <filesystem>

namespace wgrd::downloader {
enum class StorageRole {
	SeedRead,
	FetchWrite
};

enum class ReadOutcome {
	Success,
	Absent,
	Fault
};

class ChunkRangeIo {
public:
	explicit ChunkRangeIo(const ChunkLocator& locator);

	[[nodiscard]] static libtorrent::storage_error IoFailure();

	[[nodiscard]] static libtorrent::storage_error AbsentFailure();

	[[nodiscard]] static libtorrent::storage_error ReadError(
		ReadOutcome outcome,
		libtorrent::file_index_t file
	);

	[[nodiscard]] ReadOutcome Read(
		const libtorrent::file_storage& files,
		const std::filesystem::path& savePath,
		StorageRole role,
		libtorrent::piece_index_t piece,
		std::int64_t offset,
		std::int64_t length,
		char* target,
		libtorrent::file_index_t& failedFile
	) const;

	[[nodiscard]] bool Write(
		const libtorrent::file_storage& files,
		const std::filesystem::path& savePath,
		StorageRole role,
		libtorrent::piece_index_t piece,
		std::int64_t offset,
		std::int64_t length,
		const char* source
	) const;

private:
	[[nodiscard]] static bool WriteFileRange_(
		const std::filesystem::path& target,
		std::uint64_t offset,
		std::int64_t length,
		const char* source
	);

	[[nodiscard]] static bool ReadFileRange_(
		const std::filesystem::path& source,
		std::uint64_t offset,
		std::int64_t length,
		char* target
	);

	const ChunkLocator* _locator;
};
}
