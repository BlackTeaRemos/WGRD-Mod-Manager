#include "gui/screens/CatalogScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"

#include <array>
#include <format>
#include <string>

namespace wgrd::gui {

namespace {

constexpr std::array<std::string_view, 3> FILTERS = {"all", "installed", "manifest held"};

}

bool CatalogScreen::Matches_(const domain::CatalogRow& row, std::size_t filter) {
    if (filter == 1) {
        return row.installed;
    }
    if (filter == 2) {
        return row.manifestHeld;
    }
    return true;
}

void CatalogScreen::DrawRow_(
    const ScreenArea& area,
    float cursorY,
    const domain::CatalogRow& row,
    ApplicationState& state,
    domain::IInstallService* install) const {

    const ImVec2 rowTopLeft(area.origin.x, cursorY);
    const ImVec2 rowBottomRight(area.origin.x + area.width, cursorY + ROW_HEIGHT);

    bool hovered = false;
    const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);

    if (hovered) {
        design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
    }

    design::TextAt(
        ImVec2(rowTopLeft.x + 6.0f, cursorY + 10.0f),
        row.installed ? "*" : "-",
        row.installed ? tokens::SUCCESS : tokens::TEXT_DISABLED,
        11.0f);

    design::TextAt(ImVec2(rowTopLeft.x + 26.0f, cursorY + 5.0f), row.modName, tokens::SECONDARY, 12.0f);
    design::TextAt(
        ImVec2(rowTopLeft.x + 26.0f, cursorY + 18.0f),
        row.publisher,
        tokens::SECONDARY_MUTED,
        10.0f);

    design::TextAt(
        ImVec2(rowTopLeft.x + 420.0f, cursorY + 10.0f),
        std::format("v{}", row.version),
        tokens::SECONDARY,
        11.0f);

    if (row.manifestHeld) {
        design::TextAt(
            ImVec2(rowTopLeft.x + 500.0f, cursorY + 10.0f),
            ScreenToolbar::FormatBytes(row.totalBytes),
            tokens::SECONDARY,
            11.0f);

        design::TextAt(
            ImVec2(rowTopLeft.x + 600.0f, cursorY + 10.0f),
            std::format("{} chunks", row.chunkCount),
            tokens::SECONDARY_MUTED,
            10.0f);

        design::TextAt(
            ImVec2(rowTopLeft.x + 700.0f, cursorY + 10.0f),
            std::format("{} files", row.fileCount),
            tokens::SECONDARY_MUTED,
            10.0f);
    } else {
        design::TextAt(
            ImVec2(rowTopLeft.x + 500.0f, cursorY + 10.0f),
            "manifest on demand",
            tokens::ADVISORY,
            10.0f);
    }

    design::TextAt(
        ImVec2(rowTopLeft.x + 790.0f, cursorY + 10.0f),
        row.installed ? "INSTALLED" : "ANNOUNCED",
        row.installed ? tokens::SUCCESS : tokens::ACCENT,
        9.0f);

    ImVec2 actionTopLeft(rowBottomRight.x - 80.0f, cursorY + 6.0f);
    ImVec2 actionBottomRight(actionTopLeft.x, actionTopLeft.y);

    if (install != nullptr) {
        const std::string_view label = row.installed ? "reinstall" : "install";
        const ImVec2 size = design::ButtonSize(label);
        actionTopLeft = ImVec2(rowBottomRight.x - size.x - 8.0f, cursorY + 6.0f);
        actionBottomRight = ImVec2(actionTopLeft.x + size.x, actionTopLeft.y + size.y);

        const bool busy = install->Progress().Busy();

        if (design::Button(
                actionTopLeft,
                label,
                busy ? design::ButtonVariant::Neutral : design::ButtonVariant::Accent,
                !busy) && !busy) {
            const auto started = install->Start(row.identifier);
            (void)started;
        }
    }

    design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

    if (clicked && !design::PointerInside(actionTopLeft, actionBottomRight)) {
        state.OpenDetail(row.identifier);
    }
}

void CatalogScreen::Draw(
    const ScreenArea& area,
    ApplicationState& state,
    domain::ICatalogService* catalog,
    domain::IInstallService* install) {
    if (catalog == nullptr) {
        const float cursorY = ScreenToolbar::Draw(area, "CATALOG", "catalog unavailable");
        ScreenToolbar::Placeholder(area, cursorY, "no data directory");
        return;
    }

    const std::vector<domain::CatalogRow>& rows = catalog->Rows();

    const std::string context = std::format(
        "{} announced - {} keys registered - {} rejected",
        rows.size(),
        catalog->RegisteredKeys(),
        catalog->RejectedCount());

    float cursorY = ScreenToolbar::Draw(area, "CATALOG", context);

    float filterX = area.origin.x + 6.0f;
    const float filterY = cursorY + 5.0f;

    for (std::size_t index = 0; index < FILTERS.size(); ++index) {
        const bool active = state.CatalogFilter() == index;
        const ImVec2 size = design::ButtonSize(FILTERS[index]);

        if (design::Button(
                ImVec2(filterX, filterY),
                FILTERS[index],
                active ? design::ButtonVariant::Accent : design::ButtonVariant::Neutral,
                active)) {
            state.SetCatalogFilter(index);
        }

        filterX += size.x + 6.0f;
    }

    if (design::Button(ImVec2(filterX + 12.0f, filterY), "refresh", design::ButtonVariant::Neutral)) {
        catalog->Refresh();
        return;
    }

    cursorY += 28.0f;

    design::HorizontalRule(area.origin.x, area.origin.x + area.width, cursorY, tokens::BORDER);
    design::TextAt(ImVec2(area.origin.x + 26.0f, cursorY + 4.0f), "MOD", tokens::SECONDARY_MUTED, 9.0f);
    design::TextAt(ImVec2(area.origin.x + 420.0f, cursorY + 4.0f), "VERSION", tokens::SECONDARY_MUTED, 9.0f);
    design::TextAt(ImVec2(area.origin.x + 500.0f, cursorY + 4.0f), "SIZE", tokens::SECONDARY_MUTED, 9.0f);
    design::TextAt(ImVec2(area.origin.x + 600.0f, cursorY + 4.0f), "CHUNKS", tokens::SECONDARY_MUTED, 9.0f);
    design::TextAt(ImVec2(area.origin.x + 700.0f, cursorY + 4.0f), "FILES", tokens::SECONDARY_MUTED, 9.0f);
    design::TextAt(ImVec2(area.origin.x + 790.0f, cursorY + 4.0f), "STATE", tokens::SECONDARY_MUTED, 9.0f);
    cursorY += 16.0f;

    std::size_t shown = 0;
    for (const domain::CatalogRow& row : rows) {
        if (!Matches_(row, state.CatalogFilter())) {
            continue;
        }

        DrawRow_(area, cursorY, row, state, install);
        cursorY += ROW_HEIGHT;
        ++shown;
    }

    if (shown == 0) {
        ScreenToolbar::Placeholder(area, cursorY, "nothing announced yet - publish a mod first");
    }
}

}
