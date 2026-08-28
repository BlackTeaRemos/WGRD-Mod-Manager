#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wgrd::manager {
class VersionNumber {
public:
	VersionNumber() noexcept;

	VersionNumber(std::uint32_t major, std::uint32_t minor, std::uint32_t patch) noexcept;

	[[nodiscard]] static std::optional<VersionNumber> Parse(std::string_view text);

	[[nodiscard]] std::uint32_t Major() const noexcept;

	[[nodiscard]] std::uint32_t Minor() const noexcept;

	[[nodiscard]] std::uint32_t Patch() const noexcept;

	[[nodiscard]] std::string ToText() const;

	auto operator<=>(const VersionNumber& other) const noexcept = default;

	bool operator==(const VersionNumber& other) const noexcept = default;

private:
	std::uint32_t _major;
	std::uint32_t _minor;
	std::uint32_t _patch;
};
}
