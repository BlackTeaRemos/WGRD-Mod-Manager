#include "app/session/GameLocationRequest.h"
#include "app/session/ManagerSession.h"
#include "manager/update/ExecutableSwapper.h"

#include <windows.h>
#include <utility>

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	wgrd::manager::ExecutableSwapper::DiscardRetired();
	wgrd::app::GameLocationRequest location;
	auto installation = wgrd::app::GameLocationRequest::Detect();
	while (true) {
		wgrd::app::ManagerSession session(installation, location);
		const int result = session.Run();
		auto selected = location.Take();
		if (!selected.has_value()) {
			if (result != 0) {
				MessageBoxW(nullptr, L"Application could not start", L"WGRD Mod Manager", MB_OK | MB_ICONERROR);
			}
			return result;
		}
		installation = std::move(selected);
	}
}
