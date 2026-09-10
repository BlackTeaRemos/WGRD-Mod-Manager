#pragma once

#include "domain/interfaces/services/IBaselineIndexService.h"
#include "gui/screens/ScreenArea.h"

namespace wgrd::gui {
class BaselineProgressPanel {
public:
	[[nodiscard]] static float Draw(const ScreenArea& area, float top, const domain::IBaselineIndexService* baseline);
};
}
