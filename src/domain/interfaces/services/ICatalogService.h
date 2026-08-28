#pragma once

#include "domain/types/status/CatalogRow.h"

#include <cstddef>
#include <vector>

namespace wgrd::domain {

class ICatalogService {
public:
    virtual ~ICatalogService() = 0;

    virtual void Refresh() = 0;

    [[nodiscard]] virtual const std::vector<CatalogRow>& Rows() const = 0;

    [[nodiscard]] virtual std::size_t RegisteredKeys() const = 0;

    [[nodiscard]] virtual std::size_t RejectedCount() const = 0;
};

inline ICatalogService::~ICatalogService() = default;

}
