#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/services/ICatalogService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/screens/order/OrderDragController.h"

#include <string>

namespace wgrd::gui {
class OrderEntryList {
public:
	static constexpr float ROW_HEIGHT = 34.0f;
	[[nodiscard]] bool Draw(const ScreenArea& area, float top, float height, float right,
		const domain::OrderSnapshot& snapshot, domain::IOrderService* order,
		domain::ICatalogService* catalog);
	[[nodiscard]] const std::string& Selected() const;
	[[nodiscard]] float Bottom() const;

private:
	[[nodiscard]] static bool Unverified_(domain::ICatalogService* catalog, const domain::InstallFolder& folder);
	[[nodiscard]] static const domain::Annotation* AnnotationFor_(
		const domain::OrderSnapshot& snapshot, const domain::InstallFolder& folder);

	std::string _selected;
	OrderDragController _drag;
	float _bottom = 0.0f;
};
}