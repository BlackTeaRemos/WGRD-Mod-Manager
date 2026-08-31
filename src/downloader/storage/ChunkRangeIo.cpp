#include "downloader/storage/ChunkRangeIo.h"

#include <libtorrent/error_code.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <vector>

namespace wgrd::downloader {
ChunkRangeIo::ChunkRangeIo(const ChunkLocator& locator)
	: _locator(&locator) {}

libtorrent::storage_error ChunkRangeIo::IoFailure() {
	libtorrent::storage_error failure;
	failure.ec = libtorrent::error_code(
		boost::system::errc::io_error,
		libtorrent::generic_category()
	);
	return failure;
}

libtorrent::storage_error ChunkRangeIo::AbsentFailure() {
	libtorrent::storage_error failure;
	failure.ec = libtorrent::error_code(
		boost::system::errc::no_such_file_or_directory,
		libtorrent::generic_category()
	);
	return failure;
}

libtorrent::storage_error ChunkRangeIo::ReadError(
	const ReadOutcome outcome,
	const libtorrent::file_index_t file
) {
	libtorrent::storage_error failure = outcome == ReadOutcome::Absent ? AbsentFailure() : IoFailure();

	failure.file(file);
	failure.operation = libtorrent::operation_t::file_read;

	return failure;
}

bool ChunkRangeIo::WriteFileRange_(
	const std::filesystem::path& target,
	const std::uint64_t offset,
	const std::int64_t length,
	const char* source
) {
	std::error_code failure;
	std::filesystem::create_directories(target.parent_path(), failure);

	if (!std::filesystem::exists(target, failure)) {
		std::ofstream created(target, std::ios::binary);
		if (!created) {
			return false;
		}
	}

	std::fstream output(target, std::ios::binary | std::ios::in | std::ios::out);
	if (!output) {
		return false;
	}

	output.seekp(static_cast<std::streamoff>(offset));
	output.write(source, length);

	return static_cast<bool>(output);
}

bool ChunkRangeIo::ReadFileRange_(
	const std::filesystem::path& source,
	const std::uint64_t offset,
	const std::int64_t length,
	char* target
) {
	std::ifstream input(source, std::ios::binary);
	if (!input) {
		return false;
	}

	input.seekg(static_cast<std::streamoff>(offset));
	input.read(target, length);

	return input.gcount() == length;
}

ReadOutcome ChunkRangeIo::Read(
	const libtorrent::file_storage& files,
	const std::filesystem::path& savePath,
	const StorageRole role,
	const libtorrent::piece_index_t piece,
	const std::int64_t offset,
	const std::int64_t length,
	char* target,
	libtorrent::file_index_t& failedFile
) const {
	const std::vector<libtorrent::file_slice> slices = files.map_block(piece, offset, length);

	std::int64_t written = 0;

	for (const libtorrent::file_slice& slice : slices) {
		if (files.pad_file_at(slice.file_index)) {
			std::fill_n(target + written, slice.size, char{0});
			written += slice.size;
			continue;
		}

		failedFile = slice.file_index;

		const std::string relative = files.file_path(slice.file_index);

		if (role == StorageRole::FetchWrite) {
			const std::vector<ChunkLocation> destinations = _locator->FindDestinations(relative);

			if (destinations.empty()) {
				if (!ReadFileRange_(
					savePath / relative,
					static_cast<std::uint64_t>(slice.offset),
					slice.size,
					target + written
				)) {
					return ReadOutcome::Absent;
				}

				written += slice.size;
				continue;
			}

			const ChunkLocation& destination = destinations.front();

			if (slice.offset + slice.size > static_cast<std::int64_t>(destination.length)) {
				return ReadOutcome::Fault;
			}

			if (!ReadFileRange_(
				destination.file,
				destination.offset + static_cast<std::uint64_t>(slice.offset),
				slice.size,
				target + written
			)) {
				return ReadOutcome::Absent;
			}

			written += slice.size;
			continue;
		}

		const std::optional<ChunkLocation> location = _locator->Find(relative);
		if (!location.has_value()) {
			return ReadOutcome::Fault;
		}

		if (slice.offset + slice.size > static_cast<std::int64_t>(location->length)) {
			return ReadOutcome::Fault;
		}

		if (!ReadFileRange_(
			location->file,
			location->offset + static_cast<std::uint64_t>(slice.offset),
			slice.size,
			target + written
		)) {
			return ReadOutcome::Fault;
		}

		written += slice.size;
	}

	return written == length ? ReadOutcome::Success : ReadOutcome::Fault;
}

bool ChunkRangeIo::Write(
	const libtorrent::file_storage& files,
	const std::filesystem::path& savePath,
	const StorageRole role,
	const libtorrent::piece_index_t piece,
	const std::int64_t offset,
	const std::int64_t length,
	const char* source
) const {
	if (role != StorageRole::FetchWrite) {
		return false;
	}

	const std::vector<libtorrent::file_slice> slices = files.map_block(piece, offset, length);

	std::int64_t consumed = 0;

	for (const libtorrent::file_slice& slice : slices) {
		if (files.pad_file_at(slice.file_index)) {
			consumed += slice.size;
			continue;
		}

		const std::string relative = files.file_path(slice.file_index);

		const std::vector<ChunkLocation> locations = _locator->FindDestinations(relative);

		if (!locations.empty()) {
			for (const ChunkLocation& location : locations) {
				if (slice.offset + slice.size > static_cast<std::int64_t>(location.length)) {
					return false;
				}

				if (!WriteFileRange_(
					location.file,
					location.offset + static_cast<std::uint64_t>(slice.offset),
					slice.size,
					source + consumed
				)) {
					return false;
				}
			}

			consumed += slice.size;
			continue;
		}

		if (!WriteFileRange_(
			savePath / relative,
			static_cast<std::uint64_t>(slice.offset),
			slice.size,
			source + consumed
		)) {
			return false;
		}

		consumed += slice.size;
	}

	return consumed == length;
}
}
