#pragma once

#include <filesystem>
#include <string_view>

namespace wgrd::domain {
class BaselinePackLayout {
public:
	static constexpr std::string_view PACK_ROOT = "Data/WarGame/PC";

	[[nodiscard]] static std::filesystem::path PackRoot(const std::filesystem::path& gameRoot);

	[[nodiscard]] static std::filesystem::path CounterpartFor(
		const std::filesystem::path& gameRoot,
		std::string_view payloadPath
	);
};
}
