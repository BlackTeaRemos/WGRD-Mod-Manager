#pragma once

#include "gui/modals/Modals.h"
#include "gui/platform/Win32Window.h"
#include "gui/render/IRenderBackend.h"
#include "gui/screens/Screens.h"
#include "gui/shell/NavigationRail.h"
#include "gui/shell/StatusBar.h"
#include "gui/shell/TitleBar.h"
#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"

#include <cstdint>
#include <memory>

namespace wgrd::gui {
class Application {
public:
	Application() = default;
	~Application();

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	[[nodiscard]] bool Start(const ApplicationServices& services);
	[[nodiscard]] int Run();

	[[nodiscard]] HWND WindowHandle() const;

private:
	void DrawFrame_();

	void AdoptFinishedInstalls_();

	Win32Window _window;
	std::unique_ptr<IRenderBackend> _backend;
	ApplicationState _state;
	TitleBar _titleBar;
	NavigationRail _rail;
	StatusBar _statusBar;
	Screens _screens;
	Modals _modals;
	ApplicationServices _services;
	std::uint64_t _seenInstalls = 0;
	bool _started = false;
};
}
