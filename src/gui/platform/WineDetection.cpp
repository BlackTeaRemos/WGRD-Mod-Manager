#include "gui/platform/WineDetection.h"

#include <Windows.h>

namespace wgrd::gui {
bool RunningUnderWine() noexcept {
	const HMODULE nativeLayer = GetModuleHandleW(L"ntdll.dll");
	if (nativeLayer == nullptr) {
		return false;
	}

	return GetProcAddress(nativeLayer, "wine_get_version") != nullptr;
}
}
