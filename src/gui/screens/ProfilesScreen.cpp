#include "gui/screens/ProfilesScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/fixtures/Fixtures.h"
#include "gui/screens/ScreenToolbar.h"

#include <span>
#include <string_view>

namespace wgrd::gui {

float ProfilesScreen::DrawList_(
    const ScreenArea& area,
    float cursorY,
    ApplicationState& state) const {

    float listY = cursorY;
    std::size_t index = 0;

    for (const fixtures::ProfileRow& row : fixtures::Profiles()) {
        const ImVec2 rowTopLeft(area.origin.x, listY);
        const ImVec2 rowBottomRight(area.origin.x + LIST_WIDTH, listY + ROW_HEIGHT);

        bool hovered = false;
        const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);
        const bool selected = state.SelectedProfile() == index;

        if (selected) {
            design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_ACTIVE_FILL);
            design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::ACCENT);
        } else if (hovered) {
            design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
        }

        design::TextAt(ImVec2(rowTopLeft.x + 6.0f, listY + 5.0f), row.name, tokens::SECONDARY, 12.0f);
        design::TextAt(ImVec2(rowTopLeft.x + 6.0f, listY + 19.0f), row.meta, tokens::SECONDARY_MUTED, 10.0f);

        const ImU32 badgeColor =
            std::string_view(row.badge) == "ACTIVE" ? tokens::SUCCESS :
            (std::string_view(row.badge) == "MISSING FILES" ? tokens::FAILURE : tokens::SECONDARY_MUTED);
        const float badgeWidth = design::MeasureText(row.badge, 9.0f).x;
        design::TextAt(
            ImVec2(rowBottomRight.x - badgeWidth - 6.0f, listY + 6.0f),
            row.badge,
            badgeColor,
            9.0f);

        design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

        if (clicked) {
            state.SelectProfile(index);
        }

        listY += ROW_HEIGHT;
        ++index;
    }

    return listY;
}

void ProfilesScreen::DrawDetail_(
    const ScreenArea& area,
    float cursorY,
    ApplicationState& state) const {

    const float detailX = area.origin.x + LIST_WIDTH;

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(detailX, cursorY),
        ImVec2(detailX, area.origin.y + area.height),
        tokens::BORDER);

    const std::span<const fixtures::ProfileRow> profiles = fixtures::Profiles();
    const std::size_t selected = state.SelectedProfile() < profiles.size() ? state.SelectedProfile() : 0;
    const fixtures::ProfileRow& active = profiles[selected];

    const float detailWidth = area.width - LIST_WIDTH;
    design::HeadingBar(ImVec2(detailX, cursorY), detailWidth, active.name, design::HeadingLevel::Primary);

    float detailY = cursorY + design::HeadingHeight(design::HeadingLevel::Primary);
    design::HeadingBar(ImVec2(detailX, detailY), detailWidth, "LOAD ORDER", design::HeadingLevel::Minor);
    detailY += design::HeadingHeight(design::HeadingLevel::Minor);

    for (const fixtures::OrderRow& row : fixtures::Order()) {
        design::TextAt(ImVec2(detailX + 6.0f, detailY + 4.0f), row.index, tokens::TEXT_DISABLED, 10.0f);
        design::TextAt(ImVec2(detailX + 30.0f, detailY + 3.0f), row.name, tokens::SECONDARY, 12.0f);
        design::TextAt(
            ImVec2(detailX + detailWidth - 90.0f, detailY + 4.0f),
            row.folder,
            tokens::SECONDARY_MUTED,
            10.0f);
        detailY += 20.0f;
        design::HorizontalRule(detailX, detailX + detailWidth, detailY, tokens::BORDER_SUBTLE);
    }

    design::Button(
        ImVec2(detailX + 6.0f, detailY + 8.0f),
        active.primaryAction,
        design::ButtonVariant::Accent,
        true,
        12.0f);

    const float primaryWidth = design::ButtonSize(active.primaryAction, 12.0f).x;
    design::Button(
        ImVec2(detailX + 12.0f + primaryWidth, detailY + 8.0f),
        "clone",
        design::ButtonVariant::Neutral,
        false,
        10.0f);
}

void ProfilesScreen::Draw(const ScreenArea& area, ApplicationState& state) {
    const float cursorY = ScreenToolbar::Draw(area, "PROFILES", "fixture data - not wired yet");

    const float listBottom = DrawList_(area, cursorY, state);
    (void)listBottom;

    DrawDetail_(area, cursorY, state);
}

}
