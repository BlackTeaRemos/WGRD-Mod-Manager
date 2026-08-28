#include "manager/scan/ModFolderScanner.h"

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

        installed.push_back(domain::InstalledMod{*folder, ReadBuilds_(entry.path())});
    }

    std::sort(installed.begin(), installed.end(), [](const auto& left, const auto& right) {
        return left.folder.Value() < right.folder.Value();
    });

    return installed;
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

    std::sort(builds.begin(), builds.end());
    return builds;
}

}
