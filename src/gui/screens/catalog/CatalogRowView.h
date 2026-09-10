#pragma once

#include "domain/types/status/CatalogRow.h"
#include "gui/screens/ScreenArea.h"
#include "gui/screens/catalog/CatalogRowAction.h"

namespace wgrd::gui {
class CatalogRowView {
public:
	static constexpr float HEIGHT = 32.0f;

	static void Header(const ScreenArea& area, float top);
	[[nodiscard]] static CatalogRowAction Draw(const ScreenArea& area, float top,
		const domain::CatalogRow& row, bool available, bool busy);
};
}
