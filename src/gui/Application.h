#pragma once

#include "gui/state/ApplicationServices.h"
#include "gui/modals/Modals.h"
#include "gui/platform/Win32Window.h"
#include "gui/render/IRenderBackend.h"
#include "gui/screens/Screens.h"
#include "gui/shell/NavigationRail.h"
#include "gui/shell/StatusBar.h"
#include "gui/shell/TitleBar.h"
#include "gui/state/ApplicationState.h"

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

private:
    void DrawFrame_();

    Win32Window _window;
    std::unique_ptr<IRenderBackend> _backend;
    ApplicationState _state;
    TitleBar _titleBar;
    NavigationRail _rail;
    StatusBar _statusBar;
    Screens _screens;
    Modals _modals;
    ApplicationServices _services;
    bool _started = false;
};

}
