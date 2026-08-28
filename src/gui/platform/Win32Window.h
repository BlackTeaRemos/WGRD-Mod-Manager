#pragma once

#include <Windows.h>

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

	void SetResizeHandler(std::function<void(std::uint32_t, std::uint32_t)> handler);

	void MoveBy(int deltaX, int deltaY);

	void Minimize();
	void ToggleMaximize();
	void Close();

private:
	static LRESULT CALLBACK RouteMessage_(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	LRESULT HandleMessage_(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	static LRESULT HitTest_(HWND window, LPARAM lparam);
	void ApplyFrameAppearance_();

	HWND _handle = nullptr;
	HINSTANCE _instance = nullptr;
	std::uint32_t _width = 0;
	std::uint32_t _height = 0;
	bool _closed = false;
	std::function<void(std::uint32_t, std::uint32_t)> _onResize;
};
}
