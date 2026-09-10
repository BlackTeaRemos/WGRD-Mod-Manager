#include "gui/platform/window/tray/NativeTrayIcon.h"

#include <windowsx.h>

namespace wgrd::gui {
NativeTrayIcon::~NativeTrayIcon() {
	Remove();
}

bool NativeTrayIcon::Show(const HWND window, const HICON icon) {
	if (_visible) {
		return true;
	}

	_description = {};
	_description.cbSize = sizeof(_description);
	_description.hWnd = window;
	_description.uID = 1;
	_description.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
	_description.uCallbackMessage = MESSAGE;
	_description.hIcon = icon;
	wcscpy_s(_description.szTip, L"WGRD Mod Manager");
	_visible = Shell_NotifyIconW(NIM_ADD, &_description) != FALSE;
	if (_visible) {
		_description.uVersion = NOTIFYICON_VERSION_4;
		Shell_NotifyIconW(NIM_SETVERSION, &_description);
	}
	return _visible;
}

void NativeTrayIcon::Remove() {
	if (_visible) {
		Shell_NotifyIconW(NIM_DELETE, &_description);
		_visible = false;
	}
}

void NativeTrayIcon::RestoreWindow() {
	const HWND window = _description.hWnd;
	Remove();
	ShowWindow(window, IsZoomed(window) ? SW_SHOWMAXIMIZED : SW_RESTORE);
	SetForegroundWindow(window);
}

void NativeTrayIcon::ExplorerRestarted() {
	if (!_visible) {
		return;
	}
	_visible = false;
	if (!Show(_description.hWnd, _description.hIcon)) {
		RestoreWindow();
	}
}

bool NativeTrayIcon::Visible() const {
	return _visible;
}

bool NativeTrayIcon::Handle(const WPARAM position, const LPARAM notification) {
	const UINT event = LOWORD(notification);
	if (event == NIN_SELECT || event == NIN_KEYSELECT || event == WM_LBUTTONDBLCLK) {
		RestoreWindow();
		return false;
	}
	if (event != WM_CONTEXTMENU) {
		return false;
	}

	POINT pointer{GET_X_LPARAM(position), GET_Y_LPARAM(position)};
	if (pointer.x == -1 && pointer.y == -1) {
		GetCursorPos(&pointer);
	}
	HMENU menu = CreatePopupMenu();
	if (menu == nullptr) {
		return false;
	}
	AppendMenuW(menu, MF_STRING, 1, L"Show manager");
	AppendMenuW(menu, MF_STRING, 2, L"Exit");
	SetForegroundWindow(_description.hWnd);
	const UINT selected = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pointer.x, pointer.y, _description.hWnd, nullptr);
	DestroyMenu(menu);
	PostMessageW(_description.hWnd, WM_NULL, 0, 0);
	if (selected == 1 || selected == 2) {
		RestoreWindow();
	} else {
		Shell_NotifyIconW(NIM_SETFOCUS, &_description);
	}
	return selected == 2;
}
}
