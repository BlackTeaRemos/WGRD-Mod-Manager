#pragma once

#include "domain/types/status/InstallProgress.h"

#include <windows.h>
#include <shobjidl.h>

namespace wgrd::gui {
class NativeTaskbarProgress {
public:
	NativeTaskbarProgress() = default;
	~NativeTaskbarProgress();

	NativeTaskbarProgress(const NativeTaskbarProgress&) = delete;
	NativeTaskbarProgress& operator=(const NativeTaskbarProgress&) = delete;

	void Initialize(HWND window);
	void Created();
	void Update(const domain::InstallProgress& progress);
	void Reset();

private:
	HWND _window = nullptr;
	ITaskbarList3* _taskbar = nullptr;
	bool _ownsApartment = false;
	bool _ready = false;
	domain::InstallPhase _lastPhase = domain::InstallPhase::Idle;
};
}
