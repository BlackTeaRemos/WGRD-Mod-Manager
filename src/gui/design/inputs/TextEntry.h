#pragma once

#include <imgui.h>

#include <cstddef>
#include <string_view>

namespace wgrd::gui::design {
class TextEntry {
public:
	[[nodiscard]] static bool Draw(ImVec2 topLeft, float width, std::string_view identity,
		char* buffer, std::size_t capacity, ImGuiInputTextFlags flags);
};
}
