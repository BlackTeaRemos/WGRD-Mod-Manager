#pragma once

#include "gui/fonts/FontRole.h"

#include <imgui.h>
#include <array>

namespace wgrd::gui::design {
class FontLibrary {
public:
	[[nodiscard]] static bool Build();
	[[nodiscard]] static ImFont* For(FontRole role);

private:
	inline static std::array<ImFont*, 5> _fonts{};
};
}
