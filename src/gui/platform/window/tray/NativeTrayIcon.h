#pragma once

#include <windows.h>
#include <shellapi.h>

namespace wgrd::gui {
class NativeTrayIcon {
public:
	static constexpr UINT MESSAGE = WM_APP + 41;

	NativeTrayIcon() = default;
	~NativeTrayIcon();

	NativeTrayIcon(const NativeTrayIcon&) = delete;
	NativeTrayIcon& operator=(const NativeTrayIcon&) = delete;

	[[nodiscard]] bool Show(HWND window, HICON icon);
	void Remove();
	void RestoreWindow();
	void ExplorerRestarted();
	[[nodiscard]] bool Handle(WPARAM position, LPARAM notification);
	[[nodiscard]] bool Visible() const;

private:
	NOTIFYICONDATAW _description{};
	bool _visible = false;
};
}
