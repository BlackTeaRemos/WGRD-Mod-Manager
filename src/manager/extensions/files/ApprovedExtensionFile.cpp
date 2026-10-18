#include "manager/extensions/files/ApprovedExtensionFile.h"
#include "manager/extensions/ExtensionPackageContract.h"
#include "manager/extensions/SignedExtensionVerifier.h"

#include <system_error>
#include <utility>

namespace wgrd::manager {
std::expected<MappedFile, domain::ExtensionPackageError> ApprovedExtensionFile::Open(
    const std::filesystem::path& path
) {
    std::error_code failure;
    const std::uintmax_t size = std::filesystem::file_size(path, failure);
    if (failure || size < ExtensionPackageContract::MIN_PACKAGE_BYTES
        || size > ExtensionPackageContract::MAX_PACKAGE_BYTES) {
        return std::unexpected(domain::ExtensionPackageError::InvalidPackage);
    }
    auto mapped = MappedFile::Open(path);
    if (!mapped.has_value()) {
        return std::unexpected(domain::ExtensionPackageError::InvalidPackage);
    }
    const SignedExtensionVerifier verifier;
    const auto approved = verifier.Verify(mapped->Data());
    if (!approved.has_value()) {
        return std::unexpected(approved.error());
    }
    return std::move(*mapped);
}
}
