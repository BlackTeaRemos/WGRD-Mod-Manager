#pragma once

#include "domain/types/status/RegistryStatus.h"

namespace wgrd::domain {

class IRegistryUpdater {
public:
    virtual ~IRegistryUpdater() = 0;

    [[nodiscard]] virtual RegistryStatus Status() const = 0;

    virtual void Poll() = 0;
};

inline IRegistryUpdater::~IRegistryUpdater() = default;

}
