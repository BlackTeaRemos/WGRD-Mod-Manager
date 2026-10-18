#include "manager/extensions/ExtensionInstallApproval.h"
#include "manager/extensions/ExtensionPackageContract.h"
#include "manager/extensions/files/ApprovedExtensionFile.h"

#include <system_error>

namespace wgrd::manager {
bool ExtensionInstallApproval::Verify(
    const domain::InstallPlan& plan,
    const std::filesystem::path& modFolder,
    const std::string_view stagingSuffix,
    const domain::IContentHasher& hasher
) {
    std::filesystem::path candidate = modFolder / ExtensionPackageContract::FILE_NAME;
    bool replacing = false;
    const domain::FilePlan* replacement = nullptr;
    for (const domain::FilePlan& file : plan.Files()) {
        if (file.path == ExtensionPackageContract::FILE_NAME) {
            if (replacing) {
                return false;
            }
            candidate = std::filesystem::path(candidate.string() + std::string(stagingSuffix));
            replacing = true;
            replacement = &file;
        }
    }
    if (!replacing) {
        for (const std::string& removal : plan.Removals()) {
            if (removal == ExtensionPackageContract::FILE_NAME) {
                return true;
            }
        }
        std::error_code failure;
        const bool exists = std::filesystem::exists(candidate, failure);
        if (failure) {
            return false;
        }
        if (!exists) {
            return true;
        }
    }
    const auto approved = ApprovedExtensionFile::Open(candidate);
    if (!approved.has_value()) {
        return false;
    }
    if (replacement != nullptr) {
        const auto content = approved->Data();
        if (content.size() != replacement->size) {
            return false;
        }
        std::uint64_t coveredBytes = 0;
        for (const domain::ChunkPlacement& placement : replacement->placements) {
            if (placement.targetOffset != coveredBytes || placement.length == 0
                || placement.targetOffset > content.size()
                || placement.length > content.size() - placement.targetOffset
                || hasher.Hash(content.subspan(placement.targetOffset, placement.length)) != placement.digest) {
                return false;
            }
            coveredBytes += placement.length;
        }
        if (coveredBytes != content.size()) {
            return false;
        }
    }
    return true;
}
}
