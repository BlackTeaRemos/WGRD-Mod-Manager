#pragma once

#include "domain/types/preferences/WindowPreferences.h"
#include "gui/platform/window/frame/NativeWindowFrame.h"
#include "gui/platform/window/taskbar/NativeTaskbarProgress.h"
#include "gui/platform/window/tray/NativeTrayIcon.h"

#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>

namespace wgrd::gui {
class Win32Window {
public:
	Win32Window() = default;
	~Win32Window();

	Win32Window(const Win32Window&) = delete;
	Win32Window& operator=(const Win32Window&) = delete;

	[[nodiscard]] bool Create(const std::wstring& title, std::uint32_t width, std::uint32_t height);
	void Destroy();
	[[nodiscard]] bool PumpMessages();
	[[nodiscard]] HWND Handle() const noexcept;
	[[nodiscard]] std::uint32_t Width() const noexcept;
	[[nodiscard]] std::uint32_t Height() const noexcept;
	[[nodiscard]] float Scale() const noexcept;
	[[nodiscard]] bool Maximized() const;
	[[nodiscard]] bool Visible() const;

	void SetResizeHandler(std::function<void(std::uint32_t, std::uint32_t)> handler);
	void SetCaptionRegions(RECT dragRegion, RECT maximizeRegion);
	void Minimize();
	void ToggleMaximize();
	void Close(bool explicitExit = false);
	[[nodiscard]] bool CloseRequested() const;
	[[nodiscard]] bool ExplicitExitRequested() const;
	void AcknowledgeClose();
	void Background(bool useTray);
	void HideForShutdown();
	void UpdateProgress(const domain::InstallProgress& progress);
	void RestorePlacement(const domain::WindowPreferences& preferences);
	[[nodiscard]] domain::WindowPreferences CapturePlacement(bool backgroundOnClose) const;

private:
	static LRESULT CALLBACK RouteMessage_(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	LRESULT HandleMessage_(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

	HWND _handle = nullptr;
	HINSTANCE _instance = nullptr;
	HICON _icon = nullptr;
	HICON _smallIcon = nullptr;
	std::uint32_t _width = 0;
	std::uint32_t _height = 0;
	bool _closed = false;
	bool _closeRequested = false;
	bool _explicitExit = false;
	bool _maximizePressed = false;
	std::function<void(std::uint32_t, std::uint32_t)> _onResize;
	NativeWindowFrame _frame;
	NativeTaskbarProgress _taskbar;
	NativeTrayIcon _tray;
	UINT _taskbarCreated = RegisterWindowMessageW(L"TaskbarButtonCreated");
	UINT _explorerCreated = RegisterWindowMessageW(L"TaskbarCreated");
};
}
