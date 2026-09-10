#pragma once

#include "gui/state/ApplicationServices.h"
#include "gui/state/ApplicationState.h"

#include <string>

namespace wgrd::gui {
class ModDetailsPanel {
public:
	void Draw(ApplicationState& state, const ApplicationServices& services);

private:
	void Start_(bool verify, const std::string& identifier, ApplicationState& state,
		const ApplicationServices& services);
	std::string _notice;
	std::string _shownIdentifier;
	bool _retryInstall = false;
	bool _verify = false;
};
}
