#pragma once

#include "domain/types/order/Annotation.h"
#include "domain/types/order/InstalledMod.h"
#include "domain/types/order/LoadOrder.h"

#include <span>
#include <vector>

namespace wgrd::domain {

class IOrderInspector {
public:
    virtual ~IOrderInspector() = 0;

    [[nodiscard]] virtual std::vector<Annotation> Inspect(
        const LoadOrder& order,
        std::span<const InstalledMod> installed) const = 0;
};

inline IOrderInspector::~IOrderInspector() = default;

}
