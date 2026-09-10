#pragma once

#include "domain/types/preferences/WindowPreferences.h"

#include <windows.h>

namespace wgrd::gui {
class NativeWindowPlacement {
public:
	static void Restore(HWND window, const domain::WindowPreferences& preferences);

	[[nodiscard]] static domain::WindowPreferences Capture(HWND window, bool backgroundOnClose);
};
}
