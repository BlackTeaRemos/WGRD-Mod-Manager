#include "gui/shell/StatusBar.h"

#include "domain/BuildInfo.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"

#include <format>
#include <string>

namespace wgrd::gui {

void StatusBar::Draw(ImVec2 origin, float width, const ApplicationServices& services) {
    const ImVec2 bottomRight(origin.x + width, origin.y + tokens::STATUS_BAR_HEIGHT);

    design::FillRect(origin, bottomRight, tokens::SURFACE_RAISED);
    design::HorizontalRule(origin.x, bottomRight.x, origin.y, tokens::BORDER);

    const float baseline = origin.y + 5.0f;
    float cursor = origin.x + 8.0f;

    const std::string index =
        std::string(domain::build::INDEX_REPOSITORY) + " @ " + std::string(domain::build::COMMIT);
    design::TextAt(ImVec2(cursor, baseline), index, tokens::SECONDARY_MUTED, 10.0f);
    cursor += design::MeasureText(index, 10.0f).x + 8.0f;

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(cursor, origin.y),
        ImVec2(cursor, bottomRight.y),
        tokens::BORDER);
    cursor += 8.0f;

    if (services.catalog != nullptr) {
        const std::string trust = std::format(
            "{} keys - {} announced",
            services.catalog->RegisteredKeys(),
            services.catalog->Rows().size());

        design::TextAt(ImVec2(cursor, baseline), trust, tokens::ACCENT, 10.0f);
        cursor += design::MeasureText(trust, 10.0f).x + 8.0f;

        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(cursor, origin.y),
            ImVec2(cursor, bottomRight.y),
            tokens::BORDER);
        cursor += 8.0f;
    }

    if (services.order != nullptr) {
        const domain::OrderSnapshot& snapshot = services.order->Current();

        design::TextAt(ImVec2(cursor, baseline), "load_order.txt", tokens::ACCENT, 10.0f);
        cursor += design::MeasureText("load_order.txt", 10.0f).x + 6.0f;

        design::TextAt(
            ImVec2(cursor, baseline),
            snapshot.writable ? "writable" : "read only",
            snapshot.writable ? tokens::SUCCESS : tokens::FAILURE,
            10.0f);
    } else {
        design::TextAt(ImVec2(cursor, baseline), "game not found", tokens::FAILURE, 10.0f);
    }

    const std::string release = domain::build::RELEASE_REPOSITORY.empty()
        ? std::string("local build")
        : std::string(domain::build::RELEASE_REPOSITORY);

    const float releaseWidth = design::MeasureText(release, 10.0f).x;
    design::TextAt(
        ImVec2(bottomRight.x - releaseWidth - 8.0f, baseline),
        release,
        tokens::SECONDARY_MUTED,
        10.0f);
}

}
