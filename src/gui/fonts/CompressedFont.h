#pragma once

#include <imgui.h>
#include <span>

namespace wgrd::gui {
class CompressedFont {
public:
	[[nodiscard]] static ImFont* Add(std::span<const unsigned char> compressed, const char* name);
};
}
