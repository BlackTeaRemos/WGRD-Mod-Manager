#include "manager/scan/ModFolderScanner.h"

#include "manager/install/ContentInstaller.h"
#include "manager/scan/ModMetadataReader.h"

#include <algorithm>
#include <system_error>

namespace wgrd::manager {
std::vector<domain::InstalledMod> ModFolderScanner::Scan(const std::filesystem::path& modsDirectory) {
	std::vector<domain::InstalledMod> installed;

	std::error_code listingError;
	std::filesystem::directory_iterator entries(modsDirectory, listingError);
	if (listingError) {
		return installed;
	}

	for (const std::filesystem::directory_entry& entry : entries) {
		if (!entry.is_directory()) {
			continue;
		}

		const auto folder = domain::InstallFolder::Parse(entry.path().filename().string());
		if (!folder) {
			continue;
		}

		if (!HoldsPayload(entry.path())) {
			continue;
		}

		installed.push_back(domain::InstalledMod{
				*folder, ReadBuilds_(entry.path()), ModMetadataReader::Read(entry.path())
			}
		);
	}

	std::ranges::sort(installed, [](const auto& left, const auto& right) {
		          return left.folder.Value() < right.folder.Value();
	          }
	);

	return installed;
}

bool ModFolderScanner::HoldsPayload(const std::filesystem::path& modDirectory) {
	std::error_code walking;
	std::filesystem::recursive_directory_iterator walker(modDirectory, walking);
	const std::filesystem::recursive_directory_iterator walkEnd;

	if (walking) {
		return false;
	}

	bool sawStaging = false;

	for (; walker != walkEnd; walker.increment(walking)) {
		if (walking) {
			return true;
		}

		if (!walker->is_regular_file(walking) || walking) {
			continue;
		}

		if (walker->path().filename().string().ends_with(ContentInstaller::STAGING_SUFFIX)) {
			sawStaging = true;
			continue;
		}

		return true;
	}

	return !sawStaging;
}

std::vector<domain::GameBuild> ModFolderScanner::ReadBuilds_(const std::filesystem::path& modDirectory) {
	std::vector<domain::GameBuild> builds;

	std::error_code listingError;
	std::filesystem::directory_iterator entries(modDirectory, listingError);
	if (listingError) {
		return builds;
	}

	for (const std::filesystem::directory_entry& entry : entries) {
		if (!entry.is_directory()) {
			continue;
		}

		const auto build = domain::GameBuild::Parse(entry.path().filename().string());
		if (build) {
			builds.push_back(*build);
		}
	}

	std::ranges::sort(builds);
	return builds;
}
}
