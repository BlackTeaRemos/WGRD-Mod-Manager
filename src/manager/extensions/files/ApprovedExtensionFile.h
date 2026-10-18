#pragma once

#include "domain/interfaces/content/extensions/IExtensionPackageVerifier.h"
#include "manager/io/MappedFile.h"

namespace wgrd::manager {
class ApprovedExtensionFile {
public:
    [[nodiscard]] static std::expected<MappedFile, domain::ExtensionPackageError> Open(
        const std::filesystem::path& path
    );

private:
    ApprovedExtensionFile() = delete;
};
}
