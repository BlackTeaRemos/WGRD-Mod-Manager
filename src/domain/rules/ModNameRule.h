#pragma once

#include <cstddef>
#include <string_view>

namespace wgrd::domain {
class ModNameRule {
public:
	static constexpr std::size_t MAXIMUM_LENGTH = 64;

	[[nodiscard]] static bool IsAcceptable(std::string_view modName);

private:
	ModNameRule() = delete;
};
}
