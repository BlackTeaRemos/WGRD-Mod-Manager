#pragma once

#include "domain/types/status/CatalogRow.h"
#include "gui/screens/catalog/CatalogSort.h"

#include <string_view>
#include <vector>

namespace wgrd::gui {
class CatalogQuery {
public:
	[[nodiscard]] static std::vector<const domain::CatalogRow*> Select(
		const std::vector<domain::CatalogRow>& rows, std::string_view search, std::size_t filter, CatalogSort sort);

private:
	[[nodiscard]] static std::string Fold_(std::string_view text);
};
}
