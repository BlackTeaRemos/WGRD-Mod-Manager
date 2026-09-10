#include "gui/platform/window/frame/NativeWindowFrame.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>

namespace wgrd::gui {
void NativeWindowFrame::Apply(const HWND window) {
	constexpr DWORD CORNERS = DWMWCP_DONOTROUND;
	DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &CORNERS, sizeof(CORNERS));

	constexpr COLORREF BORDER = DWMWA_COLOR_NONE;
	DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &BORDER, sizeof(BORDER));
}

void NativeWindowFrame::SetCaption(const RECT dragRegion, const RECT maximizeRegion) {
	_dragRegion = dragRegion;
	_maximizeRegion = maximizeRegion;
}

void NativeWindowFrame::Constrain(const HWND window, MINMAXINFO& limits) {
	MONITORINFO monitor{sizeof(MONITORINFO)};
	if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
		return;
	}

	const UINT dpi = GetDpiForWindow(window);
	const LONG availableWidth = monitor.rcWork.right - monitor.rcWork.left;
	const LONG availableHeight = monitor.rcWork.bottom - monitor.rcWork.top;
	limits.ptMinTrackSize.x = std::min<LONG>(MulDiv(500, dpi, 96), availableWidth);
	limits.ptMinTrackSize.y = std::min<LONG>(MulDiv(420, dpi, 96), availableHeight);
	limits.ptMaxPosition.x = monitor.rcWork.left - monitor.rcMonitor.left;
	limits.ptMaxPosition.y = monitor.rcWork.top - monitor.rcMonitor.top;
	limits.ptMaxSize.x = availableWidth;
	limits.ptMaxSize.y = availableHeight;
}

LRESULT NativeWindowFrame::HitTest(const HWND window, const LPARAM position) const {
	RECT bounds{};
	if (!GetWindowRect(window, &bounds)) {
		return HTCLIENT;
	}

	POINT pointer{GET_X_LPARAM(position), GET_Y_LPARAM(position)};
	if (!IsZoomed(window)) {
		const UINT dpi = GetDpiForWindow(window);
		const int margin = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
		const bool left = pointer.x < bounds.left + margin;
		const bool right = pointer.x >= bounds.right - margin;
		const bool top = pointer.y < bounds.top + margin;
		const bool bottom = pointer.y >= bounds.bottom - margin;
		if (top && left) {
			return HTTOPLEFT;
		}
		if (top && right) {
			return HTTOPRIGHT;
		}
		if (bottom && left) {
			return HTBOTTOMLEFT;
		}
		if (bottom && right) {
			return HTBOTTOMRIGHT;
		}
		if (top) {
			return HTTOP;
		}
		if (bottom) {
			return HTBOTTOM;
		}
		if (left) {
			return HTLEFT;
		}
		if (right) {
			return HTRIGHT;
		}
	}

	ScreenToClient(window, &pointer);
	if (PtInRect(&_maximizeRegion, pointer)) {
		return HTMAXBUTTON;
	}
	if (PtInRect(&_dragRegion, pointer)) {
		return HTCAPTION;
	}
	return HTCLIENT;
}

void NativeWindowFrame::ShowSystemMenu(const HWND window, const POINT position) {
	HMENU menu = GetSystemMenu(window, FALSE);
	if (menu == nullptr) {
		return;
	}

	const bool maximized = IsZoomed(window) != FALSE;
	EnableMenuItem(menu, SC_RESTORE, MF_BYCOMMAND | (maximized ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));
	EnableMenuItem(menu, SC_SIZE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));
	EnableMenuItem(menu, SC_MOVE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));
	const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, position.x, position.y, window, nullptr);
	if (command != 0) {
		PostMessageW(window, WM_SYSCOMMAND, command, 0);
	}
}
}
