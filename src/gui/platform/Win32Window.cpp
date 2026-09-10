#include "gui/platform/Win32Window.h"

#include "gui/platform/WindowIconResource.h"
#include "gui/platform/window/dpi/DpiViewport.h"
#include "gui/platform/window/placement/NativeWindowPlacement.h"

#include <imgui_impl_win32.h>
#include <windowsx.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace wgrd::gui {
namespace {
	constexpr wchar_t WINDOW_CLASS_NAME[] = L"WgrdModManagerWindow";
	constexpr DWORD WINDOW_STYLE = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
}

Win32Window::~Win32Window() {
	Destroy();
}

bool Win32Window::Create(const std::wstring& title, const std::uint32_t width, const std::uint32_t height) {
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	_instance = GetModuleHandleW(nullptr);
	const UINT dpi = GetDpiForSystem();
	_icon = static_cast<HICON>(LoadImageW(_instance, MAKEINTRESOURCEW(WGRD_WINDOW_ICON), IMAGE_ICON,
		GetSystemMetricsForDpi(SM_CXICON, dpi), GetSystemMetricsForDpi(SM_CYICON, dpi), LR_DEFAULTCOLOR));
	_smallIcon = static_cast<HICON>(LoadImageW(_instance, MAKEINTRESOURCEW(WGRD_WINDOW_ICON), IMAGE_ICON,
		GetSystemMetricsForDpi(SM_CXSMICON, dpi), GetSystemMetricsForDpi(SM_CYSMICON, dpi), LR_DEFAULTCOLOR));

	WNDCLASSEXW description{sizeof(WNDCLASSEXW)};
	description.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	description.lpfnWndProc = RouteMessage_;
	description.hInstance = _instance;
	description.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	description.lpszClassName = WINDOW_CLASS_NAME;
	description.hIcon = _icon;
	description.hIconSm = _smallIcon;
	if (RegisterClassExW(&description) == 0) {
		return false;
	}

	RECT bounds{0, 0, MulDiv(static_cast<int>(width), dpi, 96), MulDiv(static_cast<int>(height), dpi, 96)};
	_handle = CreateWindowExW(0, WINDOW_CLASS_NAME, title.c_str(), WINDOW_STYLE, CW_USEDEFAULT, CW_USEDEFAULT,
		bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, _instance, this);
	if (_handle == nullptr) {
		return false;
	}

	RECT client{};
	GetClientRect(_handle, &client);
	_width = static_cast<std::uint32_t>(client.right);
	_height = static_cast<std::uint32_t>(client.bottom);
	NativeWindowFrame::Apply(_handle);
	NativeWindowPlacement::Restore(_handle, domain::WindowPreferences{});
	_taskbar.Initialize(_handle);
	ShowWindow(_handle, SW_SHOWDEFAULT);
	UpdateWindow(_handle);
	return true;
}

void Win32Window::Destroy() {
	_tray.Remove();
	_taskbar.Reset();
	if (_handle != nullptr) {
		DestroyWindow(_handle);
		_handle = nullptr;
	}
	if (_instance != nullptr) {
		UnregisterClassW(WINDOW_CLASS_NAME, _instance);
		_instance = nullptr;
	}
	if (_icon != nullptr) {
		DestroyIcon(_icon);
		_icon = nullptr;
	}
	if (_smallIcon != nullptr) {
		DestroyIcon(_smallIcon);
		_smallIcon = nullptr;
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

float Win32Window::Scale() const noexcept {
	return static_cast<float>(GetDpiForWindow(_handle)) / 96.0f;
}

bool Win32Window::Maximized() const {
	return IsZoomed(_handle) != FALSE;
}

bool Win32Window::Visible() const {
	return IsWindowVisible(_handle) && !IsIconic(_handle);
}

void Win32Window::SetResizeHandler(std::function<void(std::uint32_t, std::uint32_t)> handler) {
	_onResize = std::move(handler);
}

void Win32Window::SetCaptionRegions(const RECT dragRegion, const RECT maximizeRegion) {
	const float scale = Scale();
	_frame.SetCaption(
		RECT{static_cast<LONG>(dragRegion.left * scale), static_cast<LONG>(dragRegion.top * scale),
			static_cast<LONG>(dragRegion.right * scale), static_cast<LONG>(dragRegion.bottom * scale)},
		RECT{static_cast<LONG>(maximizeRegion.left * scale), static_cast<LONG>(maximizeRegion.top * scale),
			static_cast<LONG>(maximizeRegion.right * scale), static_cast<LONG>(maximizeRegion.bottom * scale)}
	);
}

void Win32Window::Minimize() {
	ShowWindow(_handle, SW_MINIMIZE);
}

void Win32Window::ToggleMaximize() {
	ShowWindow(_handle, Maximized() ? SW_RESTORE : SW_MAXIMIZE);
}

void Win32Window::Close(const bool explicitExit) {
	_closeRequested = true;
	_explicitExit = explicitExit;
}

bool Win32Window::CloseRequested() const {
	return _closeRequested;
}

bool Win32Window::ExplicitExitRequested() const {
	return _explicitExit;
}

void Win32Window::AcknowledgeClose() {
	_closeRequested = false;
	_explicitExit = false;
}

void Win32Window::Background(const bool useTray) {
	if (useTray && _tray.Show(_handle, _smallIcon)) {
		ShowWindow(_handle, SW_HIDE);
	} else {
		Minimize();
	}
}

void Win32Window::HideForShutdown() {
	_tray.Remove();
	ShowWindow(_handle, SW_HIDE);
}

void Win32Window::UpdateProgress(const domain::InstallProgress& progress) {
	_taskbar.Update(progress);
}

void Win32Window::RestorePlacement(const domain::WindowPreferences& preferences) {
	NativeWindowPlacement::Restore(_handle, preferences);
}

domain::WindowPreferences Win32Window::CapturePlacement(const bool backgroundOnClose) const {
	return NativeWindowPlacement::Capture(_handle, backgroundOnClose);
}

LRESULT CALLBACK Win32Window::RouteMessage_(const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) {
	if (message == WM_NCCREATE) {
		const auto* creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
	}
	auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (self != nullptr) {
		return self->HandleMessage_(window, message, wparam, lparam);
	}
	return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT Win32Window::HandleMessage_(const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) {
	if (message == _taskbarCreated) {
		_taskbar.Created();
		return 0;
	}
	if (message == _explorerCreated) {
		_tray.ExplorerRestarted();
		return 0;
	}
	if (message == NativeTrayIcon::MESSAGE) {
		if (_tray.Handle(wparam, lparam)) {
			Close(true);
		}
		return 0;
	}
	DpiViewport::BeginNativeInput();
	const LRESULT handled = ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam);
	DpiViewport::EndNativeInput();
	if (handled) {
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
		case WM_ERASEBKGND:
			return 1;
		case WM_NCHITTEST:
			return _frame.HitTest(window, lparam);
		case WM_GETMINMAXINFO:
			NativeWindowFrame::Constrain(window, *reinterpret_cast<MINMAXINFO*>(lparam));
			return 0;
		case WM_DPICHANGED: {
			const RECT& suggested = *reinterpret_cast<RECT*>(lparam);
			SetWindowPos(window, nullptr, suggested.left, suggested.top, suggested.right - suggested.left,
				suggested.bottom - suggested.top, SWP_NOZORDER | SWP_NOACTIVATE);
			return 0;
		}
		case WM_NCLBUTTONDOWN:
			if (wparam == HTMAXBUTTON) {
				_maximizePressed = true;
				return 0;
			}
			break;
		case WM_NCLBUTTONUP:
			if (_maximizePressed) {
				_maximizePressed = false;
				if (_frame.HitTest(window, lparam) == HTMAXBUTTON) {
					ToggleMaximize();
				}
				return 0;
			}
			break;
		case WM_NCRBUTTONUP:
			if (wparam == HTCAPTION) {
				NativeWindowFrame::ShowSystemMenu(window, POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
				return 0;
			}
			break;
		case WM_SIZE:
			if (wparam != SIZE_MINIMIZED) {
				_width = LOWORD(lparam);
				_height = HIWORD(lparam);
				if (_onResize) {
					_onResize(_width, _height);
				}
			}
			return 0;
		case WM_CLOSE:
			Close();
			return 0;
		case WM_DESTROY:
			_closed = true;
			return 0;
		default:
			break;
	}
	return DefWindowProcW(window, message, wparam, lparam);
}
}
