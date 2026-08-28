#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class RevocationSignable {
public:
	static constexpr std::string_view PREFIX = "wgrd-revoke-v1";
	static constexpr std::string_view SEPARATOR = "\n";

	[[nodiscard]] static std::vector<std::uint8_t> Bytes(
		std::string_view fingerprint,
		std::string_view revokedAt,
		std::string_view reason
	);

private:
	RevocationSignable() = delete;
};
}
