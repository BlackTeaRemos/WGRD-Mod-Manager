#pragma once

#include "gui/screens/ScreenArea.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace wgrd::gui {
class ScreenToolbar {
public:
	static constexpr float HEIGHT = 24.0f;

	[[nodiscard]] static float Draw(
		const ScreenArea& area,
		std::string_view title,
		std::string_view context
	);

	static void Placeholder(const ScreenArea& area, float cursorY, std::string_view message);

	[[nodiscard]] static std::string FormatBytes(std::uint64_t bytes);
};
}
