#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class PreviewImageCodec {
public:
	[[nodiscard]] static bool IsPng(std::span<const std::uint8_t> bytes);
	[[nodiscard]] static std::string Encode(std::span<const std::uint8_t> bytes);
	[[nodiscard]] static std::optional<std::vector<std::uint8_t>> Decode(std::string_view text, std::size_t limit);
};
}
