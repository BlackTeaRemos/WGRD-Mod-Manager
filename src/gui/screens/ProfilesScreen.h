#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/order/IProfileService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

#include <array>
#include <string>

namespace wgrd::gui {
class ProfilesScreen {
public:
	static constexpr float ROW_HEIGHT = 34.0f;
	static constexpr float LIST_WIDTH = 236.0f;
	static constexpr std::size_t NAME_CAPACITY = 64;

	void Draw(
		const ScreenArea& area,
		ApplicationState& state,
		domain::IProfileService* profiles,
		domain::IOrderService* order
	);

private:
	[[nodiscard]] static ImVec2 HeadingOrigin_(const ScreenArea& area, float cursorY);

	[[nodiscard]] static float HeadingWidth_(const ScreenArea& area);

	[[nodiscard]] float DrawList_(
		const ScreenArea& area,
		float cursorY,
		ApplicationState& state,
		domain::IProfileService* profiles
	);

	[[nodiscard]] float DrawGameProfiles_(
		const ScreenArea& area,
		float cursorY,
		domain::IProfileService* profiles
	);

	void DrawDetail_(
		const ScreenArea& area,
		float cursorY,
		ApplicationState& state,
		domain::IProfileService* profiles,
		domain::IOrderService* order
	);

	[[nodiscard]] const domain::ProfileSummary* Selected_(
		const ApplicationState& state,
		domain::IProfileService* profiles
	) const;

	std::array<char, NAME_CAPACITY> _nameBuffer{};
	std::string _notice;
	bool _selectionPrimed = false;
};
}
