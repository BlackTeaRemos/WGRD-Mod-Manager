#pragma once

#include <cstdint>

namespace wgrd::domain {
struct WindowPreferences {
	std::int32_t left = 0;
	std::int32_t top = 0;
	std::int32_t width = 1320;
	std::int32_t height = 770;
	std::uint32_t dpi = 96;
	bool positioned = false;
	bool maximized = false;
	bool backgroundOnClose = false;
};
}
