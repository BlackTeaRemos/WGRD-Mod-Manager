#include "gui/platform/Win32Window.h"

#include "gui/platform/WindowIconResource.h"

#include <dwmapi.h>
#include <imgui_impl_win32.h>
#include <windowsx.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace wgrd::gui {
namespace {
	constexpr wchar_t WINDOW_CLASS_NAME[] = L"WgrdModManagerWindow";
	constexpr DWORD WINDOW_STYLE = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
}

Win32Window::~Win32Window() {
	Destroy();
}

bool Win32Window::Create(const std::wstring& title, const std::uint32_t width, const std::uint32_t height) {
	_instance = GetModuleHandleW(nullptr);

	WNDCLASSEXW description{};
	description.cbSize = sizeof(description);
	description.style = CS_HREDRAW | CS_VREDRAW;
	description.lpfnWndProc = RouteMessage_;
	description.hInstance = _instance;
	description.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	description.lpszClassName = WINDOW_CLASS_NAME;

	description.hIcon = static_cast<HICON>(LoadImageW(
		_instance,
		MAKEINTRESOURCEW(WGRD_WINDOW_ICON),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXICON),
		GetSystemMetrics(SM_CYICON),
		LR_DEFAULTCOLOR
	));

	description.hIconSm = static_cast<HICON>(LoadImageW(
		_instance,
		MAKEINTRESOURCEW(WGRD_WINDOW_ICON),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR
	));

	RegisterClassExW(&description);

	RECT bounds{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	AdjustWindowRectEx(&bounds, WINDOW_STYLE, FALSE, 0);

	_handle = CreateWindowExW(
		0,
		WINDOW_CLASS_NAME,
		title.c_str(),
		WINDOW_STYLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		bounds.right - bounds.left,
		bounds.bottom - bounds.top,
		nullptr,
		nullptr,
		_instance,
		this
	);

	if (_handle == nullptr) {
		return false;
	}

	_width = width;
	_height = height;

	ApplyFrameAppearance_();

	ShowWindow(_handle, SW_SHOWDEFAULT);
	UpdateWindow(_handle);
	return true;
}

LRESULT Win32Window::HitTest_(const HWND window, const LPARAM lparam) {
	constexpr int RESIZE_MARGIN = 6;

	RECT bounds{};
	if (!GetWindowRect(window, &bounds)) {
		return HTCLIENT;
	}

	const POINT pointer{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

	const bool nearLeft = pointer.x < bounds.left + RESIZE_MARGIN;
	const bool nearRight = pointer.x >= bounds.right - RESIZE_MARGIN;
	const bool nearTop = pointer.y < bounds.top + RESIZE_MARGIN;
	const bool nearBottom = pointer.y >= bounds.bottom - RESIZE_MARGIN;

	if (nearTop && nearLeft) {
		return HTTOPLEFT;
	}
	if (nearTop && nearRight) {
		return HTTOPRIGHT;
	}
	if (nearBottom && nearLeft) {
		return HTBOTTOMLEFT;
	}
	if (nearBottom && nearRight) {
		return HTBOTTOMRIGHT;
	}
	if (nearTop) {
		return HTTOP;
	}
	if (nearBottom) {
		return HTBOTTOM;
	}
	if (nearLeft) {
		return HTLEFT;
	}
	if (nearRight) {
		return HTRIGHT;
	}

	return HTCLIENT;
}

void Win32Window::ApplyFrameAppearance_() {
	constexpr DWORD cornerPreference = DWMWCP_DONOTROUND;
	DwmSetWindowAttribute(
		_handle,
		DWMWA_WINDOW_CORNER_PREFERENCE,
		&cornerPreference,
		sizeof(cornerPreference)
	);

	constexpr COLORREF borderColor = DWMWA_COLOR_NONE;
	DwmSetWindowAttribute(_handle, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
}

void Win32Window::Destroy() {
	if (_handle != nullptr) {
		DestroyWindow(_handle);
		_handle = nullptr;
	}
	if (_instance != nullptr) {
		UnregisterClassW(WINDOW_CLASS_NAME, _instance);
		_instance = nullptr;
	}
}

bool Win32Window::PumpMessages() {
	MSG message{};
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
		if (message.message == WM_QUIT) {
			_closed = true;
		}
	}

	return !_closed;
}

HWND Win32Window::Handle() const noexcept {
	return _handle;
}

std::uint32_t Win32Window::Width() const noexcept {
	return _width;
}

std::uint32_t Win32Window::Height() const noexcept {
	return _height;
}

void Win32Window::SetResizeHandler(std::function<void(std::uint32_t, std::uint32_t)> handler) {
	_onResize = std::move(handler);
}

void Win32Window::BeginSystemDrag() {
	if (_handle == nullptr) {
		return;
	}

	POINT cursor{};
	if (GetCursorPos(&cursor) == 0) {
		return;
	}

	ReleaseCapture();

	SendMessageW(
		_handle,
		WM_NCLBUTTONDOWN,
		HTCAPTION,
		MAKELPARAM(cursor.x, cursor.y)
	);
}

void Win32Window::Minimize() {
	ShowWindow(_handle, SW_MINIMIZE);
}

void Win32Window::ToggleMaximize() {
	const bool maximized = IsZoomed(_handle) != FALSE;
	ShowWindow(_handle, maximized ? SW_RESTORE : SW_MAXIMIZE);
}

void Win32Window::Close() {
	_closed = true;
}

LRESULT CALLBACK Win32Window::RouteMessage_(const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) {
	if (message == WM_NCCREATE) {
		const auto* const creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
	}

	auto* const self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (self != nullptr) {
		return self->HandleMessage_(window, message, wparam, lparam);
	}

	return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT Win32Window::HandleMessage_(const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) {
		return 1;
	}

	switch (message) {
		case WM_NCCALCSIZE:
			if (wparam == TRUE) {
				return 0;
			}
			break;

		case WM_NCACTIVATE:
			return TRUE;

		case WM_NCPAINT:
			return 0;

		case WM_ERASEBKGND:
			return 1;

		case WM_NCHITTEST:
			return HitTest_(window, lparam);

		case WM_SIZE:
			if (wparam != SIZE_MINIMIZED) {
				_width = LOWORD(lparam);
				_height = HIWORD(lparam);
				if (_onResize) {
					_onResize(_width, _height);
				}
			}
			return 0;

		case WM_SYSCOMMAND:
			if ((wparam & 0xFFF0) == SC_KEYMENU) {
				return 0;
			}
			break;

		case WM_CLOSE:
			_closed = true;
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		default:
			break;
	}

	return DefWindowProcW(window, message, wparam, lparam);
}
}
