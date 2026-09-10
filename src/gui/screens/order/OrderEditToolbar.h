#pragma once

#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationServices.h"

#include <string>

namespace wgrd::gui {
class OrderEditToolbar {
public:
	[[nodiscard]] float Draw(const ScreenArea& area, float top, const ApplicationServices& services);

private:
	std::string _notice;
	bool _noticeError = false;
};
}
