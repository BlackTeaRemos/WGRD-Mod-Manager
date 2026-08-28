#include "gui/screens/OrderScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"

#include <format>
#include <string>

namespace wgrd::gui {

bool OrderScreen::DrawEntries_(
    const ScreenArea& area,
    float cursorY,
    float rightX,
    const domain::OrderSnapshot& snapshot,
    domain::IOrderService* orderService) const {

    const float leftWidth = rightX - area.origin.x;

    design::HeadingBar(
        ImVec2(area.origin.x, cursorY),
        leftWidth,
        "Load order - later entries win",
        design::HeadingLevel::Section);

    float leftY = cursorY + design::HeadingHeight(design::HeadingLevel::Section);

    std::size_t index = 0;
    for (const domain::OrderEntryView& entry : snapshot.entries) {
        const ImVec2 rowTopLeft(area.origin.x, leftY);
        const ImVec2 rowBottomRight(rightX, leftY + ROW_HEIGHT);

        const bool blocking = entry.enabled && !entry.present;

        if (blocking) {
            design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_WARNED_ROW);
            design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::FAILURE);
        }

        const std::string position = (index < 9 ? "0" : "") + std::to_string(index + 1);

        design::TextAt(ImVec2(rowTopLeft.x + 8.0f, leftY + 8.0f), "::", tokens::TEXT_DISABLED, 11.0f);
        design::TextAt(ImVec2(rowTopLeft.x + 22.0f, leftY + 8.0f), position, tokens::TEXT_DISABLED, 11.0f);

        if (design::Checkbox(ImVec2(rowTopLeft.x + 42.0f, leftY + 6.0f), entry.enabled)) {
            orderService->SetEnabled(entry.folder, !entry.enabled);
            return true;
        }

        const ImU32 nameColor = entry.enabled ? tokens::SECONDARY : tokens::TEXT_DISABLED;
        design::TextAt(ImVec2(rowTopLeft.x + 62.0f, leftY + 4.0f), entry.folder.Value(), nameColor, 12.0f);

        const std::string location = "Mods/" + entry.folder.Value();
        design::TextAt(ImVec2(rowTopLeft.x + 62.0f, leftY + 17.0f), location, tokens::SECONDARY_MUTED, 10.0f);

        if (index > 0) {
            if (design::Button(
                    ImVec2(rowBottomRight.x - 54.0f, leftY + 6.0f),
                    "up",
                    design::ButtonVariant::Neutral)) {
                orderService->Move(index, index - 1);
                return true;
            }
        }

        if (index + 1 < snapshot.entries.size()) {
            if (design::Button(
                    ImVec2(rowBottomRight.x - 28.0f, leftY + 6.0f),
                    "dn",
                    design::ButtonVariant::Neutral)) {
                orderService->Move(index, index + 1);
                return true;
            }
        }

        if (blocking) {
            design::TextAt(ImVec2(rowTopLeft.x + 62.0f, leftY + 28.0f), "missing", tokens::FAILURE, 10.0f);
            design::TextAt(
                ImVec2(rowTopLeft.x + 110.0f, leftY + 28.0f),
                "folder not present",
                tokens::FAILURE,
                10.0f);
        }

        design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

        leftY += ROW_HEIGHT;
        ++index;
    }

    return false;
}

void OrderScreen::DrawSidebar_(
    const ScreenArea& area,
    float cursorY,
    float rightX,
    const domain::OrderSnapshot& snapshot) const {

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(rightX, cursorY),
        ImVec2(rightX, area.origin.y + area.height),
        tokens::BORDER);

    const float rightWidth = area.origin.x + area.width - rightX;
    float rightY = cursorY;

    design::HeadingBar(
        ImVec2(rightX, rightY),
        rightWidth,
        std::format("Attention - {}", snapshot.annotations.size()),
        design::HeadingLevel::Section,
        design::HeadingTone::Warning);
    rightY += design::HeadingHeight(design::HeadingLevel::Section);

    for (const domain::Annotation& annotation : snapshot.annotations) {
        const bool severe = annotation.severity == domain::AnnotationSeverity::Blocking;
        const ImU32 tone = severe ? tokens::FAILURE : tokens::ADVISORY;

        design::TextAt(ImVec2(rightX + 6.0f, rightY + 5.0f), annotation.folder.Value(), tone, 11.0f);
        design::TextAt(ImVec2(rightX + 6.0f, rightY + 19.0f), annotation.tag, tokens::SECONDARY_MUTED, 10.0f);
        design::TextAt(
            ImVec2(rightX + 50.0f, rightY + 19.0f),
            annotation.explanation,
            tokens::SECONDARY_MUTED,
            10.0f);

        rightY += 36.0f;
        design::HorizontalRule(rightX, rightX + rightWidth, rightY, tokens::BORDER_SUBTLE);
    }

    rightY += 8.0f;
    design::HeadingBar(ImVec2(rightX, rightY), rightWidth, "INSTALLED", design::HeadingLevel::Minor);
    rightY += design::HeadingHeight(design::HeadingLevel::Minor);

    for (const domain::InstalledMod& mod : snapshot.installed) {
        design::TextAt(ImVec2(rightX + 6.0f, rightY + 4.0f), mod.folder.Value(), tokens::SECONDARY, 10.0f);

        std::string builds;
        for (const domain::GameBuild& build : mod.builds) {
            builds += build.ToText();
            builds += " ";
        }
        design::TextAt(ImVec2(rightX + 150.0f, rightY + 4.0f), builds, tokens::SECONDARY_MUTED, 10.0f);

        rightY += 18.0f;
        design::HorizontalRule(rightX, rightX + rightWidth, rightY, tokens::BORDER_SUBTLE);
    }

    rightY += 8.0f;
    design::HeadingBar(ImVec2(rightX, rightY), rightWidth, "ORDER FILE", design::HeadingLevel::Minor);
    rightY += design::HeadingHeight(design::HeadingLevel::Minor);

    design::TextAt(ImVec2(rightX + 6.0f, rightY + 4.0f), snapshot.gameRoot, tokens::SECONDARY_MUTED, 10.0f);
    rightY += 16.0f;
    design::TextAt(
        ImVec2(rightX + 6.0f, rightY + 4.0f),
        snapshot.writable ? "writable" : "read only",
        snapshot.writable ? tokens::SUCCESS : tokens::FAILURE,
        10.0f);
}

void OrderScreen::Draw(const ScreenArea& area, domain::IOrderService* orderService) {
    if (orderService == nullptr) {
        const float cursorY = ScreenToolbar::Draw(area, "LOAD ORDER", "game not found");
        design::TextAt(
            ImVec2(area.origin.x + 6.0f, cursorY + 16.0f),
            "no wargame installation detected",
            tokens::FAILURE,
            12.0f);
        return;
    }

    const domain::OrderSnapshot& snapshot = orderService->Current();

    const std::string context = std::format(
        "{} entries - {} enabled",
        snapshot.entries.size(),
        snapshot.enabledCount);

    const float cursorY = ScreenToolbar::Draw(area, "LOAD ORDER", context);

    const float rightX = area.origin.x + area.width * 0.6f;

    if (DrawEntries_(area, cursorY, rightX, snapshot, orderService)) {
        return;
    }

    DrawSidebar_(area, cursorY, rightX, snapshot);
}

}
