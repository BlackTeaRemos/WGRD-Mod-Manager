#pragma once

#include "domain/interfaces/platform/IFilePicker.h"
#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IPublishService.h"
#include "gui/screens/PublishKeyStep.h"
#include "gui/screens/PublishSignStep.h"
#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

#include <string>

namespace wgrd::gui {
class PublishScreen {
public:
	static constexpr std::size_t STEP_COUNT = 3;

	void Draw(
		const ScreenArea& area,
		ApplicationState& state,
		domain::IPublishService* publish,
		domain::ICatalogService* catalog,
		const domain::IFilePicker* files
	);

private:
	[[nodiscard]] static bool StepSatisfied_(
		std::size_t index,
		const ApplicationState& state,
		domain::IPublishService* publish
	);

	[[nodiscard]] float DrawStepBar_(
		const ScreenArea& area,
		float cursorY,
		ApplicationState& state,
		domain::IPublishService* publish
	) const;

	[[nodiscard]] float DrawSourceStep_(
		const ScreenArea& area,
		float cursorY,
		ApplicationState& state,
		domain::IPublishService* publish
	) const;

	void DrawHistory_(
		const ScreenArea& area,
		float cursorY,
		domain::IPublishService* publish
	) const;

	void ClearSecrets_();

	PublishKeyStep _keyStep;
	PublishSignStep _signStep;
	std::string _notice;
};
}
