#pragma once

#include "gui/modals/Modals.h"
#include "gui/screens/Screens.h"

namespace wgrd::gui::tests {
class ScreenRenderFixture {
public:
	ScreenRenderFixture();
	~ScreenRenderFixture();
	[[nodiscard]] bool Ready() const;
	void Draw(float width, float height, ApplicationState& state);

private:
	ImGuiContext* _context;
	bool _ready;
	Screens _screens;
	Modals _modals;
};
}
