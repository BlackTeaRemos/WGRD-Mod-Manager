#pragma once

#include "domain/types/distribution/InstallPlan.h"
#include "domain/types/content/ModManifest.h"

namespace wgrd::domain {

class IManifestDiffer {
public:
    virtual ~IManifestDiffer() = 0;

    [[nodiscard]] virtual InstallPlan Diff(const ModManifest& held, const ModManifest& target) const = 0;
};

inline IManifestDiffer::~IManifestDiffer() = default;

}
