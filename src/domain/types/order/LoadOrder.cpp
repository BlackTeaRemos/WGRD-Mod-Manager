#include "domain/types/order/LoadOrder.h"

#include <algorithm>

namespace wgrd::domain {

LoadOrder::LoadOrder(std::vector<OrderEntry> entries)
    : _entries(std::move(entries)) {
}

const std::vector<OrderEntry>& LoadOrder::Entries() const noexcept {
    return _entries;
}

std::vector<InstallFolder> LoadOrder::EnabledFolders() const {
    std::vector<InstallFolder> enabled;
    enabled.reserve(_entries.size());

    for (const OrderEntry& entry : _entries) {
        if (entry.enabled) {
            enabled.push_back(entry.folder);
        }
    }

    return enabled;
}

std::size_t LoadOrder::EnabledCount() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(_entries.begin(), _entries.end(), [](const OrderEntry& entry) {
            return entry.enabled;
        }));
}

}
