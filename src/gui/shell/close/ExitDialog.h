#pragma once

#include "gui/shell/close/WindowCloseChoice.h"
#include "gui/state/ApplicationState.h"

namespace wgrd::gui {
class ExitDialog {
public:
	void Open();
	[[nodiscard]] WindowCloseChoice Draw(ApplicationState& state);

private:
	bool _requested = false;
};
}
