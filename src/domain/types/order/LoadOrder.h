#pragma once

#include "domain/types/order/InstallFolder.h"

#include <vector>

namespace wgrd::domain {
struct OrderEntry {
	InstallFolder folder;
	bool enabled;
};

class LoadOrder {
public:
	LoadOrder() = default;
	explicit LoadOrder(std::vector<OrderEntry> entries);

	[[nodiscard]] const std::vector<OrderEntry>& Entries() const noexcept;

	[[nodiscard]] std::vector<InstallFolder> EnabledFolders() const;

	[[nodiscard]] std::size_t EnabledCount() const noexcept;

private:
	std::vector<OrderEntry> _entries;
};
}
