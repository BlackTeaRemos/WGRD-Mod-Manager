#pragma once

#include <imgui.h>

#include <array>
#include <cstddef>

namespace wgrd::gui::design {
class FontLibrary {
public:
	static constexpr std::array<float, 5> SIZES = {9.0f, 10.0f, 11.0f, 12.0f, 13.0f};

	static void Build();

	[[nodiscard]] static ImFont* For(float size);

private:
	[[nodiscard]] static std::size_t NearestIndex_(float size);
};
}
