#pragma once

#include "domain/types/order/Profile.h"

#include <optional>
#include <string>
#include <string_view>

namespace wgrd::manager {
class ProfileCodec {
public:
	static constexpr std::size_t ENTRY_LIMIT = 4096;

	[[nodiscard]] static std::string Encode(const domain::Profile& profile);

	[[nodiscard]] static std::optional<domain::Profile> Decode(std::string_view document);
};
}
