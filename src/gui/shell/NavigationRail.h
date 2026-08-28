#pragma once

#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"

#include <imgui.h>

#include <cstdint>
#include <string>

namespace wgrd::gui {
class NavigationRail {
public:
	void Draw(
		ImVec2 origin,
		float height,
		ApplicationState& state,
		const ApplicationServices& services
	);

private:
	[[nodiscard]] static std::string BadgeFor_(Screen screen, const ApplicationServices& services);

	[[nodiscard]] static std::uint64_t HeldBytes_(const ApplicationServices& services);

	static void DrawFooter_(
		ImVec2 origin,
		ImVec2 bottomRight,
		float width,
		const ApplicationServices& services
	);
};
}
