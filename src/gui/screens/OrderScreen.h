#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/order/IProfileService.h"
#include "domain/interfaces/services/ICatalogService.h"
#include "gui/screens/ScreenArea.h"

#include <string>

namespace wgrd::gui {
class OrderScreen {
public:
	static constexpr float ROW_HEIGHT = 34.0f;
	static constexpr float BANNER_HEIGHT = 34.0f;

	void Draw(
		const ScreenArea& area,
		domain::IOrderService* orderService,
		domain::IProfileService* profileService,
		domain::ICatalogService* catalogService
	);

private:
	[[nodiscard]] static bool Unverified_(
		domain::ICatalogService* catalogService,
		const domain::InstallFolder& folder
	);

	[[nodiscard]] static float DrawDefaultBanner_(
		const ScreenArea& area,
		float cursorY,
		domain::IProfileService* profileService
	);

	[[nodiscard]] static const domain::Annotation* AnnotationFor_(
		const domain::OrderSnapshot& snapshot,
		const domain::InstallFolder& folder
	);

	[[nodiscard]] bool DrawEntries_(
		const ScreenArea& area,
		float cursorY,
		float rightX,
		const domain::OrderSnapshot& snapshot,
		domain::IOrderService* orderService,
		domain::ICatalogService* catalogService
	);

	void DrawSidebar_(
		const ScreenArea& area,
		float cursorY,
		float rightX,
		const domain::OrderSnapshot& snapshot
	) const;

	[[nodiscard]] float DrawDetails_(
		float left,
		float cursorY,
		float width,
		const domain::OrderSnapshot& snapshot
	) const;

	[[nodiscard]] static const domain::InstalledMod* InstalledFor_(
		const domain::OrderSnapshot& snapshot,
		const domain::InstallFolder& folder
	);

	std::string _selected;
};
}
