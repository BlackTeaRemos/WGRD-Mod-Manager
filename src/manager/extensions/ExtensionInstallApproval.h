#pragma once

#include "domain/types/distribution/InstallPlan.h"
#include "domain/interfaces/content/IContentHasher.h"
#include <string_view>

namespace wgrd::manager {
class ExtensionInstallApproval {
public:
    [[nodiscard]] static bool Verify(
        const domain::InstallPlan& plan,
        const std::filesystem::path& modFolder,
        std::string_view stagingSuffix,
        const domain::IContentHasher& hasher
    );

private:
    ExtensionInstallApproval() = delete;
};
}
