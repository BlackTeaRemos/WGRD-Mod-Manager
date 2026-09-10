#include "gui/platform/window/placement/NativeWindowPlacement.h"

#include <algorithm>

namespace wgrd::gui {
void NativeWindowPlacement::Restore(const HWND window, const domain::WindowPreferences& preferences) {
	if (preferences.positioned) {
		WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
		placement.showCmd = SW_SHOWNORMAL;
		placement.rcNormalPosition = RECT{
			preferences.left, preferences.top,
			preferences.left + preferences.width, preferences.top + preferences.height
		};
		if (!SetWindowPlacement(window, &placement)) {
			return;
		}
	}

	RECT bounds{};
	MONITORINFO monitor{sizeof(MONITORINFO)};
	if (GetWindowRect(window, &bounds) && GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor)) {
		const UINT dpi = GetDpiForWindow(window);
		const LONG availableWidth = monitor.rcWork.right - monitor.rcWork.left;
		const LONG availableHeight = monitor.rcWork.bottom - monitor.rcWork.top;
		const int savedDpi = static_cast<int>(std::max(preferences.dpi, 48u));
		const LONG width = std::clamp<LONG>(MulDiv(preferences.width, dpi, savedDpi), std::min<LONG>(MulDiv(500, dpi, 96), availableWidth), availableWidth);
		const LONG height = std::clamp<LONG>(MulDiv(preferences.height, dpi, savedDpi), std::min<LONG>(MulDiv(420, dpi, 96), availableHeight), availableHeight);
		const LONG left = std::clamp(bounds.left, monitor.rcWork.left, monitor.rcWork.right - width);
		const LONG top = std::clamp(bounds.top, monitor.rcWork.top, monitor.rcWork.bottom - height);
		SetWindowPos(window, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	if (preferences.maximized) {
		ShowWindow(window, SW_MAXIMIZE);
	}
}

domain::WindowPreferences NativeWindowPlacement::Capture(const HWND window, const bool backgroundOnClose) {
	domain::WindowPreferences preferences;
	preferences.backgroundOnClose = backgroundOnClose;
	WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
	if (!GetWindowPlacement(window, &placement)) {
		return preferences;
	}

	preferences.left = placement.rcNormalPosition.left;
	preferences.top = placement.rcNormalPosition.top;
	preferences.width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
	preferences.height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
	preferences.dpi = GetDpiForWindow(window);
	preferences.positioned = true;
	preferences.maximized = IsZoomed(window) || (IsIconic(window) && (placement.flags & WPF_RESTORETOMAXIMIZED) != 0);
	return preferences;
}
}
