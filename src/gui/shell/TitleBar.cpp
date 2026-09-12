#include "gui/shell/TitleBar.h"

#include "domain/BuildInfo.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/text/ClippedText.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/ShellText.h"

#include <algorithm>
#include <format>
#include <string>

namespace wgrd::gui {
namespace {
	constexpr float CONTROL_WIDTH = 26.0f;

	bool DrawControl(const ImVec2 position, const float width, const std::string_view glyph, const char* label) {
		const ImVec2 end(position.x + width, position.y + tokens::TITLE_BAR_HEIGHT);
		bool highlighted = false;
		const bool activated = design::RowHit(position, end, highlighted);
		if (highlighted) {
			design::FillRect(position, end, tokens::ACCENT_HOVER);
		}
		const ImVec2 size = design::MeasureText(glyph, 11.0f, FontRole::Label);
		design::TextAt(ImVec2(position.x + (width - size.x) * 0.5f, position.y + (tokens::TITLE_BAR_HEIGHT - size.y) * 0.5f),
			glyph, tokens::PRIMARY, 11.0f, FontRole::Label);
		if (highlighted && ImGui::IsWindowHovered()) {
			ImGui::SetTooltip("%s", label);
		}
		return activated;
	}
}

std::string TitleBar::FormatRates_(const ApplicationServices& services) {
	if (services.swarm == nullptr) {
		return {};
	}
	const auto& swarm = services.swarm->Status();
	return std::format(text::shell::RATE_FORMAT, ScreenToolbar::FormatBytes(swarm.downloadRate), ScreenToolbar::FormatBytes(swarm.uploadRate));
}

void TitleBar::Draw(const ImVec2 origin, const float width, ApplicationState& state,
	Win32Window& window, const ApplicationServices& services) {
	const float right = origin.x + width;
	design::FillRect(origin, ImVec2(right, origin.y + tokens::TITLE_BAR_HEIGHT), tokens::ACCENT);
	float cursor = right - CONTROL_WIDTH;
	if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, "x", "Close window")) {
		window.Close();
	}
	cursor -= CONTROL_WIDTH;
	const RECT maximize{static_cast<LONG>(cursor), static_cast<LONG>(origin.y),
		static_cast<LONG>(cursor + CONTROL_WIDTH), static_cast<LONG>(origin.y + tokens::TITLE_BAR_HEIGHT)};
	if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, window.Maximized() ? "][" : "[]",
		window.Maximized() ? "Restore window" : "Maximize window")) {
		window.ToggleMaximize();
	}
	cursor -= CONTROL_WIDTH;
	if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, "-", "Minimize window")) {
		window.Minimize();
	}
	const float settingsWidth = design::MeasureText("SETTINGS", 11.0f, FontRole::Label).x + 16.0f;
	cursor -= settingsWidth;
	if (DrawControl(ImVec2(cursor, origin.y), settingsWidth, "SETTINGS", "Settings")) {
		state.OpenSettings();
	}
	const float nameWidth = design::MeasureText(text::shell::PRODUCT, 13.0f, FontRole::Heading).x;
	design::ClippedText::Draw(ImVec2(origin.x + 8.0f, origin.y + 6.0f), cursor - origin.x - 16.0f,
		text::shell::PRODUCT, tokens::PRIMARY, 13.0f, FontRole::Heading);
	if (width >= 760.0f) {
		const std::string stamp = std::format(text::shell::STAMP_FORMAT, domain::build::VERSION, domain::build::COMMIT);
		design::ClippedText::Draw(ImVec2(origin.x + nameWidth + 18.0f, origin.y + 8.0f), 200.0f,
			stamp, tokens::PRIMARY, 10.0f);
		const std::string rates = FormatRates_(services);
		const float ratesWidth = design::MeasureText(rates, 10.0f).x;
		design::TextAt(ImVec2(cursor - ratesWidth - 10.0f, origin.y + 8.0f), rates, tokens::PRIMARY, 10.0f);
	}
	window.SetCaptionRegions(RECT{static_cast<LONG>(origin.x), static_cast<LONG>(origin.y),
		static_cast<LONG>(std::max(origin.x, cursor)), static_cast<LONG>(origin.y + tokens::TITLE_BAR_HEIGHT)}, maximize);
}
}
