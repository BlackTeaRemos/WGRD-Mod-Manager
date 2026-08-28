#include "gui/screens/ScreenToolbar.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/text/CommonText.h"

#include <array>
#include <format>

namespace wgrd::gui {
float ScreenToolbar::Draw(
	const ScreenArea& area,
	const std::string_view title,
	const std::string_view context
) {
	const ImVec2 bottomRight(area.origin.x + area.width, area.origin.y + HEIGHT);

	design::FillRect(area.origin, bottomRight, tokens::SURFACE_RAISED);
	design::HorizontalRule(area.origin.x, bottomRight.x, bottomRight.y, tokens::BORDER);

	design::TextAt(ImVec2(area.origin.x + 6.0f, area.origin.y + 7.0f), title, tokens::ACCENT, 10.0f);

	const float titleWidth = design::MeasureText(title, 10.0f).x;
	design::TextAt(
		ImVec2(area.origin.x + 14.0f + titleWidth, area.origin.y + 7.0f),
		context,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	return area.origin.y + HEIGHT;
}

void ScreenToolbar::Placeholder(const ScreenArea& area, const float cursorY, const std::string_view message) {
	design::TextAt(
		ImVec2(area.origin.x + 6.0f, cursorY + 12.0f),
		message,
		tokens::TEXT_DISABLED,
		11.0f
	);
}

std::string ScreenToolbar::FormatBytes(std::uint64_t bytes) {
	constexpr std::array<std::string_view, 4> UNITS = {text::common::BYTES, text::common::KIBIBYTES, text::common::MEBIBYTES, text::common::GIBIBYTES};

	double value = static_cast<double>(bytes);
	std::size_t unit = 0;

	while (value >= 1024.0 && unit + 1 < UNITS.size()) {
		value /= 1024.0;
		++unit;
	}

	if (unit == 0) {
		return std::format(text::common::WHOLE_UNIT_FORMAT, bytes, UNITS[unit]);
	}

	return std::format(text::common::SCALED_UNIT_FORMAT, value, UNITS[unit]);
}
}
