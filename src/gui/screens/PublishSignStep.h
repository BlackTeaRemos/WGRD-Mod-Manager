#pragma once

#include "domain/interfaces/platform/IFilePicker.h"
#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IPublishService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

#include <array>
#include <functional>
#include <string>

namespace wgrd::gui {
class PublishSignStep {
public:
	static constexpr std::size_t PASSPHRASE_CAPACITY = 128;

	[[nodiscard]] float Draw(
		const ScreenArea& area,
		float cursorY,
		ApplicationState& state,
		domain::IPublishService* publish,
		domain::ICatalogService* catalog,
		const domain::IFilePicker* files,
		std::string& notice,
		const std::function<void()>& clearSecrets
	);

	void ClearSecret();

private:
	[[nodiscard]] float DrawKeyPanel_(
		const ScreenArea& area,
		float cursorY,
		domain::IPublishService* publish,
		const domain::IFilePicker* files,
		std::string& notice,
		const std::function<void()>& clearSecrets
	);

	std::array<char, PASSPHRASE_CAPACITY> _unlockPassphrase{};
	bool _awaitingPublish = false;
};
}
