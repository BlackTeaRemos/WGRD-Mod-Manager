#include "gui/shell/TitleBar.h"

#include "domain/BuildInfo.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"

#include <string>

namespace wgrd::gui {

namespace {

constexpr float CONTROL_WIDTH = 24.0f;
constexpr float SETTINGS_PADDING = 8.0f;

struct ControlResult {
    bool clicked;
    float left;
};

ControlResult DrawControl(ImVec2 topLeft, float width, std::string_view glyph) {
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
        tokens::FromHex(0x0E0E12, 0.35f));

    const ImVec2 extent = design::MeasureText(glyph, 11.0f);
    design::TextAt(
        ImVec2(
            topLeft.x + (width - extent.x) * 0.5f,
            topLeft.y + (tokens::TITLE_BAR_HEIGHT - extent.y) * 0.5f),
        glyph,
        tokens::PRIMARY,
        11.0f);

    return ControlResult{clicked, topLeft.x};
}

}

void TitleBar::Draw(ImVec2 origin, float width, ApplicationState& state, Win32Window& window) {
    const ImVec2 bottomRight(origin.x + width, origin.y + tokens::TITLE_BAR_HEIGHT);

    design::FillRect(origin, bottomRight, tokens::ACCENT);

    design::TextAt(ImVec2(origin.x + 8.0f, origin.y + 6.0f), "WGRD MOD MANAGER", tokens::PRIMARY, 13.0f);

    const float nameWidth = design::MeasureText("WGRD MOD MANAGER", 13.0f).x;
    const std::string stamp =
        "v" + std::string(domain::build::VERSION) + " " + std::string(domain::build::COMMIT);
    design::TextAt(
        ImVec2(origin.x + 18.0f + nameWidth, origin.y + 8.0f),
        stamp,
        tokens::FromHex(0x0E0E12, 0.72f),
        10.0f);

    const std::string_view rates = "d 62.4 MB/s u 11.6 MB/s";
    const float ratesWidth = design::MeasureText(rates, 10.0f).x;

    float cursor = bottomRight.x;

    cursor -= CONTROL_WIDTH;
    if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, "x").clicked) {
        state.RequestExit();
    }

    cursor -= CONTROL_WIDTH;
    if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, "[]").clicked) {
        window.ToggleMaximize();
    }

    cursor -= CONTROL_WIDTH;
    if (DrawControl(ImVec2(cursor, origin.y), CONTROL_WIDTH, "-").clicked) {
        window.Minimize();
    }

    const float settingsWidth = design::MeasureText("SETTINGS", 10.0f).x + SETTINGS_PADDING * 2.0f;
    cursor -= settingsWidth;
    if (DrawControl(ImVec2(cursor, origin.y), settingsWidth, "SETTINGS").clicked) {
        state.OpenSettings();
    }

    design::TextAt(
        ImVec2(cursor - ratesWidth - 10.0f, origin.y + 8.0f),
        rates,
        tokens::FromHex(0x0E0E12, 0.72f),
        10.0f);

    const ImVec2 pointer = ImGui::GetIO().MousePos;
    const bool overBar =
        pointer.x >= origin.x && pointer.x < cursor - ratesWidth - 10.0f &&
        pointer.y >= origin.y && pointer.y < bottomRight.y;

    if (overBar && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        _dragging = true;
        _dragOrigin = pointer;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        _dragging = false;
    }
    if (_dragging) {
        const ImVec2 delta(pointer.x - _dragOrigin.x, pointer.y - _dragOrigin.y);
        if (delta.x != 0.0f || delta.y != 0.0f) {
            window.MoveBy(static_cast<int>(delta.x), static_cast<int>(delta.y));
        }
    }
}

}
