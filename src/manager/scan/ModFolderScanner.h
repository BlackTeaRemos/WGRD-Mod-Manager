#pragma once

#include "domain/types/order/InstalledMod.h"

#include <filesystem>
#include <vector>

namespace wgrd::manager {

class ModFolderScanner {
public:
    [[nodiscard]] static std::vector<domain::InstalledMod> Scan(const std::filesystem::path& modsDirectory);

private:
    static std::vector<domain::GameBuild> ReadBuilds_(const std::filesystem::path& modDirectory);
};

}
