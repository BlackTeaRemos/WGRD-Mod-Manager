#include "gui/shell/TitleBar.h"

#include "domain/BuildInfo.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/ShellText.h"

#include <format>
#include <string>

namespace wgrd::gui {
namespace {
	constexpr float CONTROL_WIDTH = 24.0f;
	constexpr float SETTINGS_PADDING = 8.0f;

	bool DrawControl(const ImVec2 topLeft, const float width, const std::string_view glyph) {
		const ImVec2 bottomRight(topLeft.x + width, topLeft.y + tokens::TITLE_BAR_HEIGHT);

		bool hovered = false;
		const bool clicked = design::RowHit(topLeft, bottomRight, hovered);

		if (hovered) {
			design::FillRect(topLeft, bottomRight, tokens::FromHex(0x0E0E12, 0.12f));
		}

		design::HorizontalRule(topLeft.x, topLeft.x, topLeft.y, tokens::FromHex(0x0E0E12, 0.35f));
		ImGui::GetWindowDrawList()->AddLine(
			topLeft,
			ImVec2(topLeft.x, bottomRight.y),
			tokens::FromHex(0x0E0E12, 0.35f)
		);

		const ImVec2 extent = design::MeasureText(glyph, 11.0f);
		design::TextAt(
			ImVec2(
				topLeft.x + (width - extent.x) * 0.5f,
				topLeft.y + (tokens::TITLE_BAR_HEIGHT - extent.y) * 0.5f
			),
			glyph,
			tokens::PRIMARY,
			11.0f
		);

		return clicked;
	}
}

std::string TitleBar::FormatRates_(const ApplicationServices& services) {
	if (services.swarm == nullptr) {
		return std::format(text::shell::RATE_FORMAT, ScreenToolbar::FormatBytes(0), ScreenToolbar::FormatBytes(0));
	}

	const domain::SwarmStatus& swarm = services.swarm->Status();

	return std::format(text::shell::RATE_FORMAT, ScreenToolbar::FormatBytes(swarm.downloadRate), ScreenToolbar::FormatBytes(swarm.uploadRate));
}

void TitleBar::Draw(
	const ImVec2 origin,
	const float width,
	ApplicationState& state,
	Win32Window& window,
	const ApplicationServices& services
) {
	const ImVec2 bottomRight(origin.x + width, origin.y + tokens::TITLE_BAR_HEIGHT);

	design::FillRect(origin, bottomRight, tokens::ACCENT);

	design::TextAt(ImVec2(origin.x + 8.0f, origin.y + 6.0f), text::shell::PRODUCT, tokens::PRIMARY, 13.0f);

	const float nameWidth = design::MeasureText(text::shell::PRODUCT, 13.0f).x;
	const std::string stamp =
			std::format(text::shell::STAMP_FORMAT, domain::build::VERSION, domain::build::COMMIT);
	design::TextAt(
		ImVec2(origin.x + 18.0f + nameWidth, origin.y + 8.0f),
		stamp,
		tokens::FromHex(0x0E0E12, 0.72f),
		10.0f
	);

	const std::string rates = FormatRates_(services);
	const float ratesWidth = design::MeasureText(rates, 10.0f).x;

	float cursor = bottomRight.x;

	cursor -= CONTROL_WIDTH;
	if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, text::shell::CLOSE_GLYPH)) {
		state.RequestExit();
	}

	cursor -= CONTROL_WIDTH;
	if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, text::shell::MAXIMISE_GLYPH)) {
		window.ToggleMaximize();
	}

	cursor -= CONTROL_WIDTH;
	if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, text::shell::MINIMISE_GLYPH)) {
		window.Minimize();
	}

	const float settingsWidth = design::MeasureText(text::shell::SETTINGS, 10.0f).x + SETTINGS_PADDING * 2.0f;
	cursor -= settingsWidth;
	if (DrawControl(ImVec2(cursor, origin.y), settingsWidth, text::shell::SETTINGS)) {
		state.OpenSettings();
	}

	design::TextAt(
		ImVec2(cursor - ratesWidth - 10.0f, origin.y + 8.0f),
		rates,
		tokens::FromHex(0x0E0E12, 0.72f),
		10.0f
	);

	const ImVec2 pointer = ImGui::GetIO().MousePos;
	const bool overBar =
			pointer.x >= origin.x && pointer.x < cursor - ratesWidth - 10.0f &&
			pointer.y >= origin.y && pointer.y < bottomRight.y;

	if (!overBar) {
		return;
	}

	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		window.ToggleMaximize();
		return;
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		window.BeginSystemDrag();
	}
}
}
