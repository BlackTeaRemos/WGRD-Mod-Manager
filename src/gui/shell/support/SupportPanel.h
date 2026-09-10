#pragma once

#include "gui/state/ApplicationServices.h"

#include <string>
#include <string_view>

namespace wgrd::gui {
class SupportPanel {
public:
	void Draw(const ApplicationServices& services);

private:
	void Link_(std::string_view label, std::string_view uri, const ApplicationServices& services);
	std::string _notice;
};
}
