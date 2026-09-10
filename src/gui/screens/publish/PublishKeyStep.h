#pragma once

#include "domain/interfaces/platform/IFilePicker.h"
#include "domain/interfaces/services/IPublishService.h"
#include "gui/screens/ScreenArea.h"

#include <array>
#include <functional>
#include <string>

namespace wgrd::gui {
class PublishKeyStep {
public:
	static constexpr std::size_t NAME_CAPACITY = 64;
	static constexpr std::size_t PASSPHRASE_CAPACITY = 128;

	[[nodiscard]] float Draw(
		const ScreenArea& area,
		float cursorY,
		domain::IPublishService* publish,
		const domain::IFilePicker* files,
		std::string& notice,
		const std::function<void()>& clearSecrets
	);

	void ClearSecret();

private:
	std::array<char, NAME_CAPACITY> _nameBuffer{};
	std::array<char, PASSPHRASE_CAPACITY> _createPassphrase{};
};
}
