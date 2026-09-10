#include "manager/baseline/BaselineFileScanner.h"

#include "manager/baseline/records/FileStampReader.h"
#include "manager/payload/PayloadPathPolicy.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace wgrd::manager {
bool BaselineFileScanner::IsPack_(const std::filesystem::path& file) {
	std::string extension = file.extension().string();

	std::ranges::transform(extension, extension.begin(), [](const unsigned char character) {
		                       return static_cast<char>(std::tolower(character));
	                       }
	);

	return extension == PayloadPathPolicy::PAYLOAD_EXTENSION;
}

std::vector<BaselineFileEntry> BaselineFileScanner::Scan(const std::filesystem::path& packRoot) {
	std::vector<BaselineFileEntry> entries;

	std::error_code failure;
	std::filesystem::recursive_directory_iterator walker(packRoot, failure);
	if (failure) {
		return entries;
	}

	const std::filesystem::recursive_directory_iterator end;
	for (; walker != end; walker.increment(failure)) {
		if (failure || entries.size() >= FILE_LIMIT) {
			break;
		}

		if (!walker->is_regular_file(failure) || failure) {
			continue;
		}

		if (!IsPack_(walker->path())) {
			continue;
		}

		const std::filesystem::path relative = std::filesystem::relative(walker->path(), packRoot, failure);
		if (failure) {
			continue;
		}

		const std::optional<domain::FileStamp> stamp = FileStampReader::Read(walker->path());
		if (!stamp.has_value()) {
			continue;
		}

		entries.push_back(BaselineFileEntry{relative.generic_string(), walker->path(), *stamp});
	}

	std::ranges::sort(entries, [](const BaselineFileEntry& left, const BaselineFileEntry& right) {
		                  return left.relativePath < right.relativePath;
	                  }
	);

	return entries;
}
}
