#pragma once

#include <imgui.h>

#include <cstddef>
#include <string_view>

namespace wgrd::gui::design {
class PassphraseMeter {
public:
	static constexpr std::size_t REQUIRED_LENGTH = 8;
	static constexpr std::size_t COMFORTABLE_LENGTH = 12;
	static constexpr std::size_t STRONG_OPTIONAL_COUNT = 3;

	static constexpr float LABEL_SIZE = 10.0f;
	static constexpr float CHIP_GAP = 10.0f;
	static constexpr float LINE_STEP = 14.0f;

	[[nodiscard]] static float Draw(ImVec2 topLeft, float width, std::string_view passphrase);

private:
	PassphraseMeter() = delete;
};
}
