#include "manager/update/VersionNumber.h"

#include <charconv>
#include <format>

namespace wgrd::manager {
namespace {
	constexpr std::uint32_t COMPONENT_LIMIT = 100000;

	std::optional<std::uint32_t> ParseComponent(const std::string_view text) {
		if (text.empty() || text.size() > 6) {
			return std::nullopt;
		}

		std::uint32_t value = 0;
		const char* const first = text.data();
		const char* const last = text.data() + text.size();

		const std::from_chars_result outcome = std::from_chars(first, last, value);
		if (outcome.ec != std::errc() || outcome.ptr != last) {
			return std::nullopt;
		}

		if (value >= COMPONENT_LIMIT) {
			return std::nullopt;
		}

		return value;
	}
}

VersionNumber::VersionNumber() noexcept
	: _major(0)
	, _minor(0)
	, _patch(0) {}

VersionNumber::VersionNumber(const std::uint32_t major, const std::uint32_t minor, const std::uint32_t patch) noexcept
	: _major(major)
	, _minor(minor)
	, _patch(patch) {}

std::optional<VersionNumber> VersionNumber::Parse(std::string_view text) {
	if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) {
		text.remove_prefix(1);
	}

	const std::size_t firstDot = text.find('.');
	if (firstDot == std::string_view::npos) {
		return std::nullopt;
	}

	const std::size_t secondDot = text.find('.', firstDot + 1);
	if (secondDot == std::string_view::npos) {
		return std::nullopt;
	}

	const std::optional<std::uint32_t> major = ParseComponent(text.substr(0, firstDot));
	const std::optional<std::uint32_t> minor =
			ParseComponent(text.substr(firstDot + 1, secondDot - firstDot - 1));
	const std::optional<std::uint32_t> patch = ParseComponent(text.substr(secondDot + 1));

	if (!major.has_value() || !minor.has_value() || !patch.has_value()) {
		return std::nullopt;
	}

	return VersionNumber(*major, *minor, *patch);
}

std::uint32_t VersionNumber::Major() const noexcept {
	return _major;
}

std::uint32_t VersionNumber::Minor() const noexcept {
	return _minor;
}

std::uint32_t VersionNumber::Patch() const noexcept {
	return _patch;
}

std::string VersionNumber::ToText() const {
	return std::format("{}.{}.{}", _major, _minor, _patch);
}
}
