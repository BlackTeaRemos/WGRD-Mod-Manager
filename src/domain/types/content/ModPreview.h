#pragma once

#include <cstdint>
#include <vector>

namespace wgrd::domain {
struct ModPreview {
	std::vector<std::uint8_t> thumbnail;
	std::vector<std::uint8_t> image;
	bool operator==(const ModPreview& other) const = default;
};
}
