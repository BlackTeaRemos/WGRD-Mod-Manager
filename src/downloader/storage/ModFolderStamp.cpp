#include "downloader/storage/ModFolderStamp.h"

#include <format>
#include <system_error>

namespace wgrd::downloader {
std::string ModFolderStamp::Compute(
	const domain::ModManifest& manifest,
	const std::filesystem::path& modFolder
) {
	std::string stamp;

	for (const domain::ManifestFile& file : manifest.Files()) {
		const std::filesystem::path target = modFolder / file.path;

		std::error_code failure;
		const std::uintmax_t size = std::filesystem::file_size(target, failure);

		if (failure) {
			stamp += std::format("{} {}\n", file.path, MISSING_MARK);
			continue;
		}

		failure.clear();
		const std::filesystem::file_time_type written =
				std::filesystem::last_write_time(target, failure);

		if (failure) {
			stamp += std::format("{} {}\n", file.path, MISSING_MARK);
			continue;
		}

		stamp += std::format(
			"{} {} {}\n",
			file.path,
			size,
			written.time_since_epoch().count()
		);
	}

	return stamp;
}
}
