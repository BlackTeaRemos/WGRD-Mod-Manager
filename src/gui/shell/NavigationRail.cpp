#include "gui/shell/NavigationRail.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"

#include <array>

namespace wgrd::gui {

namespace {

struct RailEntry {
    const char* key;
    const char* label;
    const char* badge;
    Screen screen;
};

constexpr std::array<RailEntry, 5> ENTRIES = {{
    {"1", "Catalog", "247", Screen::Catalog},
    {"2", "Load Order", "4/6", Screen::Order},
    {"3", "Transfers", "2", Screen::Transfers},
    {"4", "Profiles", "4", Screen::Profiles},
    {"5", "Publish", "-", Screen::Publish}
}};

constexpr float ENTRY_HEIGHT = 22.0f;

}

void NavigationRail::Draw(ImVec2 origin, float height, ApplicationState& state) {
    const float width = tokens::RAIL_WIDTH;
    const ImVec2 bottomRight(origin.x + width, origin.y + height);

    design::FillRect(origin, bottomRight, tokens::SURFACE_RAISED);

    design::FillRect(
        origin,
        ImVec2(origin.x + width, origin.y + tokens::RAIL_HEADER_HEIGHT),
        tokens::ACCENT_RAIL_HEADER);
    design::TextAt(ImVec2(origin.x + 6.0f, origin.y + 3.0f), "WORKSPACE", tokens::ACCENT, 9.0f);
    design::HorizontalRule(
        origin.x,
        bottomRight.x,
        origin.y + tokens::RAIL_HEADER_HEIGHT,
        tokens::BORDER);

    float cursorY = origin.y + tokens::RAIL_HEADER_HEIGHT;

    for (const RailEntry& entry : ENTRIES) {
        const ImVec2 rowTopLeft(origin.x, cursorY);
        const ImVec2 rowBottomRight(origin.x + width, cursorY + ENTRY_HEIGHT);

        bool hovered = false;
        const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);
        const bool active = state.ActiveScreen() == entry.screen;

        if (active) {
            design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_ACTIVE_FILL);
            design::FillRect(
                rowTopLeft,
                ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y),
                tokens::ACCENT);
        } else if (hovered) {
            design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
        }

        const ImU32 labelColor = active
            ? tokens::ACCENT
            : (hovered ? tokens::ACCENT_HOVER : tokens::SECONDARY);

        design::TextAt(
            ImVec2(rowTopLeft.x + 6.0f, cursorY + 6.0f),
            entry.key,
            tokens::SECONDARY_MUTED,
            10.0f);

        design::TextAt(ImVec2(rowTopLeft.x + 24.0f, cursorY + 5.0f), entry.label, labelColor, 12.0f);

        const float badgeWidth = design::MeasureText(entry.badge, 10.0f).x;
        design::TextAt(
            ImVec2(rowBottomRight.x - badgeWidth - 6.0f, cursorY + 6.0f),
            entry.badge,
            tokens::SECONDARY_MUTED,
            10.0f);

        design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

        if (clicked) {
            state.SetScreen(entry.screen);
        }

        cursorY += ENTRY_HEIGHT;
    }

    const float footerTop = bottomRight.y - 52.0f;
    design::HorizontalRule(origin.x, bottomRight.x, footerTop, tokens::BORDER);

    design::TextAt(ImVec2(origin.x + 6.0f, footerTop + 6.0f), "CHUNK STORE", tokens::SECONDARY_MUTED, 9.0f);
    design::TextAt(ImVec2(origin.x + 6.0f, footerTop + 18.0f), "6.9 GB", tokens::SECONDARY, 10.0f);
    design::Meter(ImVec2(origin.x + 6.0f, footerTop + 32.0f), width - 12.0f, 4.0f, 0.34f, tokens::ACCENT);
    design::TextAt(ImVec2(origin.x + 6.0f, footerTop + 39.0f), "seeding 4", tokens::SUCCESS, 10.0f);

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(bottomRight.x, origin.y),
        ImVec2(bottomRight.x, bottomRight.y),
        tokens::BORDER);
}

}
