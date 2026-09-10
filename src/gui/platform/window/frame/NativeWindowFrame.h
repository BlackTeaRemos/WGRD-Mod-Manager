#pragma once

#include <windows.h>

namespace wgrd::gui {
class NativeWindowFrame {
public:
	static void Apply(HWND window);

	static void Constrain(HWND window, MINMAXINFO& limits);

	void SetCaption(RECT dragRegion, RECT maximizeRegion);

	[[nodiscard]] LRESULT HitTest(HWND window, LPARAM position) const;

	static void ShowSystemMenu(HWND window, POINT position);

private:
	RECT _dragRegion{};
	RECT _maximizeRegion{};
};
}
