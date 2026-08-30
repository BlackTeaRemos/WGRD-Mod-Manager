#pragma once

#include "gui/state/ApplicationServices.h"

#include <imgui.h>

namespace wgrd::gui {
class StatusBar {
public:
	void Draw(ImVec2 origin, float width, const ApplicationServices& services);

private:
	[[nodiscard]] static float DrawSeparator_(ImVec2 origin, ImVec2 bottomRight, float cursor);

	[[nodiscard]] static float DrawManager_(
		ImVec2 origin,
		ImVec2 bottomRight,
		float cursor,
		const ApplicationServices& services
	);

	[[nodiscard]] static float DrawPatcher_(
		ImVec2 origin,
		float cursor,
		const ApplicationServices& services
	);

	[[nodiscard]] static ImU32 SupportTone_();

	[[nodiscard]] float DrawSupport_(ImVec2 origin, float cursor);

	void DrawSupportPanel_(ImVec2 origin, float anchorX, const ApplicationServices& services) const;

	bool _supportOpen = false;
};
}
