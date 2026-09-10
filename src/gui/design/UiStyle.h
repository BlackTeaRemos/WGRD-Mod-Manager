#pragma once

#include <imgui.h>

namespace wgrd::gui::design {
class UiStyle {
public:
	static void Apply();

private:
	static void SetColor_(ImGuiStyle& style, ImGuiCol slot, ImU32 color);
};
}
