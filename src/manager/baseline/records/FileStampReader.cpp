#include "manager/baseline/records/FileStampReader.h"

#include <system_error>

namespace wgrd::manager {
std::optional<domain::FileStamp> FileStampReader::Read(const std::filesystem::path& file) {
	std::error_code failure;

	const std::uintmax_t size = std::filesystem::file_size(file, failure);
	if (failure) {
		return std::nullopt;
	}

	const std::filesystem::file_time_type modified = std::filesystem::last_write_time(file, failure);
	if (failure) {
		return std::nullopt;
	}

	return domain::FileStamp{
		static_cast<std::uint64_t>(size), static_cast<std::int64_t>(modified.time_since_epoch().count())
	};
}
}
