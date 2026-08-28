#pragma once

#include <string_view>

namespace wgrd::manager {
class ProfileNameRule {
public:
	static constexpr std::size_t LENGTH_LIMIT = 48;

	[[nodiscard]] static bool Accepts(std::string_view name);
};
}
