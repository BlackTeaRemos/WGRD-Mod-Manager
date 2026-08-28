#pragma once

#include "domain/interfaces/platform/IFilePicker.h"
#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IPublishService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

#include <array>
#include <string>

namespace wgrd::gui {
class PublishScreen {
public:
	static constexpr std::size_t STEP_COUNT = 3;
	static constexpr std::size_t NAME_CAPACITY = 64;
	static constexpr std::size_t PASSPHRASE_CAPACITY = 128;

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

	[[nodiscard]] float DrawKeyStep_(
		const ScreenArea& area,
		float cursorY,
		domain::IPublishService* publish,
		const domain::IFilePicker* files
	);

	[[nodiscard]] float DrawSourceStep_(
		const ScreenArea& area,
		float cursorY,
		ApplicationState& state,
		domain::IPublishService* publish
	) const;

	[[nodiscard]] float DrawSignStep_(
		const ScreenArea& area,
		float cursorY,
		ApplicationState& state,
		domain::IPublishService* publish,
		domain::ICatalogService* catalog,
		const domain::IFilePicker* files
	);

	void DrawHistory_(
		const ScreenArea& area,
		float cursorY,
		domain::IPublishService* publish
	) const;

	void ClearSecrets_();

	[[nodiscard]] std::string_view Notice_(domain::IPublishService* publish) const;

	std::array<char, NAME_CAPACITY> _nameBuffer{};
	std::array<char, PASSPHRASE_CAPACITY> _createPassphrase{};
	std::array<char, PASSPHRASE_CAPACITY> _unlockPassphrase{};
	std::string _notice;
};
}
