#include "gui/platform/window/taskbar/NativeTaskbarProgress.h"

#include <algorithm>

namespace wgrd::gui {
NativeTaskbarProgress::~NativeTaskbarProgress() {
	Reset();
}

void NativeTaskbarProgress::Initialize(const HWND window) {
	Reset();
	_window = window;
	const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	_ownsApartment = SUCCEEDED(initialized);
	if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
		return;
	}

	if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_taskbar)))) {
		_taskbar->HrInit();
	}
}

void NativeTaskbarProgress::Created() {
	_ready = true;
}

void NativeTaskbarProgress::Reset() {
	if (_taskbar != nullptr) {
		_taskbar->Release();
		_taskbar = nullptr;
	}
	if (_ownsApartment) {
		CoUninitialize();
		_ownsApartment = false;
	}
	_ready = false;
	_window = nullptr;
}

void NativeTaskbarProgress::Update(const domain::InstallProgress& progress) {
	if (_window == nullptr) {
		return;
	}

	if (_ready && _taskbar != nullptr) {
		if (progress.phase == domain::InstallPhase::Planning || progress.phase == domain::InstallPhase::Installing) {
			_taskbar->SetProgressState(_window, TBPF_INDETERMINATE);
		} else if (progress.phase == domain::InstallPhase::Fetching || progress.phase == domain::InstallPhase::Verifying) {
			if (progress.remoteBytes == 0) {
				_taskbar->SetProgressState(_window, TBPF_INDETERMINATE);
			} else {
				_taskbar->SetProgressState(_window, TBPF_NORMAL);
				_taskbar->SetProgressValue(_window, std::min(progress.fetchedBytes, progress.remoteBytes), progress.remoteBytes);
			}
		} else if (progress.phase == domain::InstallPhase::Failed) {
			_taskbar->SetProgressState(_window, TBPF_ERROR);
			_taskbar->SetProgressValue(_window, 1, 1);
		} else {
			_taskbar->SetProgressState(_window, TBPF_NOPROGRESS);
		}
	}

	const bool completed = progress.phase == domain::InstallPhase::Done || progress.phase == domain::InstallPhase::Failed;
	if (completed && progress.phase != _lastPhase && GetForegroundWindow() != _window) {
		FLASHWINFO flash{sizeof(FLASHWINFO), _window, FLASHW_TRAY, 3, 0};
		FlashWindowEx(&flash);
	}
	_lastPhase = progress.phase;
}
}
