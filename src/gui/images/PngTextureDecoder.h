#pragma once

#include <imgui.h>
#include <cstdint>
#include <span>

namespace wgrd::gui {
class PngTextureDecoder {
public:
	[[nodiscard]] static bool Decode(std::span<const std::uint8_t> bytes, int maximumSize, ImTextureData& texture);
};
}
