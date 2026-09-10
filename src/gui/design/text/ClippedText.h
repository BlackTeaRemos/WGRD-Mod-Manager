#pragma once

#include "gui/fonts/FontRole.h"

#include <imgui.h>
#include <string_view>

namespace wgrd::gui::design {
class ClippedText {
public:
	static void Draw(ImVec2 origin, float width, std::string_view value, ImU32 color,
		float size, FontRole role = FontRole::Data);
};
}
