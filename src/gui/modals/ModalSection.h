#pragma once

#include <string_view>

namespace wgrd::gui {
class ModalSection {
public:
	static void Draw(std::string_view title);
};
}
