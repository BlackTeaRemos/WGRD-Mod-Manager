#include "gui/screens/PublishScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"

#include <format>
#include <string>

namespace wgrd::gui {

namespace {

constexpr std::array<std::string_view, PublishScreen::STEP_COUNT> STEPS = {
    "1 SIGNING KEY",
    "2 SOURCE",
    "3 SIGN AND ANNOUNCE"
};

constexpr float STEP_BAR_HEIGHT = 22.0f;
constexpr float CANDIDATE_ROW_HEIGHT = 22.0f;

}

float PublishScreen::DrawStepBar_(
    const ScreenArea& area,
    float cursorY,
    ApplicationState& state) const {

    const float stepWidth = area.width / static_cast<float>(STEPS.size());

    for (std::size_t index = 0; index < STEPS.size(); ++index) {
        const ImVec2 stepTopLeft(area.origin.x + stepWidth * static_cast<float>(index), cursorY);
        const ImVec2 stepBottomRight(stepTopLeft.x + stepWidth, cursorY + STEP_BAR_HEIGHT);

        bool hovered = false;
        const bool clicked = design::RowHit(stepTopLeft, stepBottomRight, hovered);
        const bool current = state.PublishStep() == index;
        const bool completed = index < state.PublishStep();

        if (current) {
            design::FillRect(stepTopLeft, stepBottomRight, tokens::ACCENT_ACTIVE_FILL);
        }

        const ImU32 color = current ? tokens::ACCENT
            : (completed ? tokens::SUCCESS : (hovered ? tokens::ACCENT_HOVER : tokens::TEXT_DISABLED));

        const float extent = design::MeasureText(STEPS[index], 10.0f).x;
        design::TextAt(
            ImVec2(stepTopLeft.x + (stepWidth - extent) * 0.5f, cursorY + 7.0f),
            STEPS[index],
            color,
            10.0f);

        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(stepBottomRight.x, cursorY),
            ImVec2(stepBottomRight.x, stepBottomRight.y),
            tokens::BORDER);

        if (clicked) {
            state.SetPublishStep(index);
        }
    }

    const float bottom = cursorY + STEP_BAR_HEIGHT;
    design::HorizontalRule(area.origin.x, area.origin.x + area.width, bottom, tokens::BORDER);

    return bottom + 10.0f;
}

float PublishScreen::DrawKeyStep_(
    const ScreenArea& area,
    float cursorY,
    domain::IPublishService* publish) {

    const domain::PublisherState& publisher = publish->Publisher();

    if (publisher.present) {
        design::TextAt(ImVec2(area.origin.x + 6.0f, cursorY), "FINGERPRINT", tokens::SECONDARY_MUTED, 10.0f);
        cursorY += 14.0f;

        design::FillRect(
            ImVec2(area.origin.x + 6.0f, cursorY),
            ImVec2(area.origin.x + area.width - 6.0f, cursorY + 24.0f),
            tokens::SURFACE_SUNKEN);
        design::StrokeRect(
            ImVec2(area.origin.x + 6.0f, cursorY),
            ImVec2(area.origin.x + area.width - 6.0f, cursorY + 24.0f),
            tokens::BORDER);
        design::TextAt(
            ImVec2(area.origin.x + 13.0f, cursorY + 7.0f),
            publisher.fingerprint,
            tokens::SECONDARY,
            11.0f);

        cursorY += 30.0f;
        design::TextAt(
            ImVec2(area.origin.x + 6.0f, cursorY),
            "ed25519 - secret key protected on disk",
            tokens::TEXT_DISABLED,
            10.0f);

        return cursorY + 22.0f;
    }

    design::TextAt(ImVec2(area.origin.x + 6.0f, cursorY), "PUBLISHER NAME", tokens::SECONDARY_MUTED, 10.0f);
    cursorY += 14.0f;

    design::TextField(
        ImVec2(area.origin.x + 6.0f, cursorY),
        area.width - 12.0f,
        "publisher",
        _nameBuffer.data(),
        _nameBuffer.size());

    cursorY += 30.0f;

    if (design::Button(
            ImVec2(area.origin.x + 6.0f, cursorY),
            "CREATE SIGNING KEY",
            design::ButtonVariant::Accent,
            true,
            12.0f)) {

        const auto created = publish->CreateKey(_nameBuffer.data());
        if (created.has_value()) {
            _nameBuffer.fill('\0');
        }
    }

    cursorY += 26.0f;
    design::TextAt(
        ImVec2(area.origin.x + 6.0f, cursorY),
        "letters digits dot underscore hyphen only",
        tokens::TEXT_DISABLED,
        10.0f);

    return cursorY + 22.0f;
}

float PublishScreen::DrawSourceStep_(
    const ScreenArea& area,
    float cursorY,
    ApplicationState& state,
    domain::IPublishService* publish) const {

    design::TextAt(ImVec2(area.origin.x + 6.0f, cursorY), "MOD FOLDER", tokens::SECONDARY_MUTED, 10.0f);
    cursorY += 16.0f;

    const std::vector<std::string>& candidates = publish->Candidates();

    if (candidates.empty()) {
        ScreenToolbar::Placeholder(area, cursorY, "no mod folders found");
        return cursorY + 34.0f;
    }

    for (const std::string& candidate : candidates) {
        const ImVec2 rowTopLeft(area.origin.x, cursorY);
        const ImVec2 rowBottomRight(area.origin.x + area.width, cursorY + CANDIDATE_ROW_HEIGHT);

        bool hovered = false;
        const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);
        const bool selected = state.PublishFolder() == candidate;

        if (selected) {
            design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_ACTIVE_FILL);
            design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::ACCENT);
        } else if (hovered) {
            design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
        }

        design::TextAt(
            ImVec2(rowTopLeft.x + 8.0f, cursorY + 5.0f),
            candidate,
            selected ? tokens::ACCENT : tokens::SECONDARY,
            11.0f);

        design::TextAt(
            ImVec2(rowTopLeft.x + 300.0f, cursorY + 5.0f),
            "Mods/" + candidate,
            tokens::SECONDARY_MUTED,
            10.0f);

        design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

        if (clicked) {
            state.SelectPublishFolder(candidate);
        }

        cursorY += CANDIDATE_ROW_HEIGHT;
    }

    return cursorY + 8.0f;
}

float PublishScreen::DrawSignStep_(
    const ScreenArea& area,
    float cursorY,
    ApplicationState& state,
    domain::IPublishService* publish,
    domain::ICatalogService* catalog) const {

    const bool keyReady = publish->Publisher().present;
    const bool folderReady = !state.PublishFolder().empty();

    design::HeadingBar(
        ImVec2(area.origin.x, cursorY),
        area.width,
        keyReady && folderReady ? "PRE-FLIGHT - READY" : "PRE-FLIGHT - INCOMPLETE",
        design::HeadingLevel::Minor,
        keyReady && folderReady ? design::HeadingTone::Success : design::HeadingTone::Warning);

    cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

    const std::array<std::pair<bool, std::string_view>, 2> checks = {{
        {keyReady, "signing key present"},
        {folderReady, "mod folder selected"}
    }};

    for (const auto& check : checks) {
        design::TextAt(
            ImVec2(area.origin.x + 6.0f, cursorY + 4.0f),
            check.first ? "ok" : "no",
            check.first ? tokens::SUCCESS : tokens::FAILURE,
            10.0f);
        design::TextAt(ImVec2(area.origin.x + 30.0f, cursorY + 3.0f), check.second, tokens::SECONDARY, 11.0f);

        cursorY += 18.0f;
        design::HorizontalRule(area.origin.x, area.origin.x + area.width, cursorY, tokens::BORDER_SUBTLE);
    }

    cursorY += 10.0f;

    if (keyReady && folderReady) {
        if (design::Button(
                ImVec2(area.origin.x + 6.0f, cursorY),
                "SIGN AND ANNOUNCE",
                design::ButtonVariant::Accent,
                true,
                12.0f)) {

            const auto published = publish->Publish(state.PublishFolder());
            if (published.has_value() && catalog != nullptr) {
                catalog->Refresh();
            }
        }
        cursorY += 26.0f;
    }

    design::TextAt(
        ImVec2(area.origin.x + 6.0f, cursorY),
        publish->LastMessage(),
        tokens::SECONDARY_MUTED,
        10.0f);

    return cursorY + 22.0f;
}

void PublishScreen::DrawHistory_(
    const ScreenArea& area,
    float cursorY,
    domain::IPublishService* publish) const {

    design::HeadingBar(
        ImVec2(area.origin.x, cursorY),
        area.width,
        "PUBLISHED THIS SESSION",
        design::HeadingLevel::Minor);

    float rowY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);

    const std::vector<domain::PublishedRelease>& history = publish->History();

    if (history.empty()) {
        ScreenToolbar::Placeholder(area, rowY, "nothing published yet");
        return;
    }

    for (const domain::PublishedRelease& release : history) {
        design::TextAt(ImVec2(area.origin.x + 6.0f, rowY + 4.0f), release.modName, tokens::SECONDARY, 11.0f);
        design::TextAt(
            ImVec2(area.origin.x + 200.0f, rowY + 4.0f),
            std::format("v{}", release.version),
            tokens::SECONDARY_MUTED,
            10.0f);
        design::TextAt(
            ImVec2(area.origin.x + 260.0f, rowY + 4.0f),
            ScreenToolbar::FormatBytes(release.totalBytes),
            tokens::SECONDARY,
            10.0f);
        design::TextAt(
            ImVec2(area.origin.x + 360.0f, rowY + 4.0f),
            std::format("{} chunks - {} files", release.chunkCount, release.fileCount),
            tokens::SECONDARY_MUTED,
            10.0f);
        design::TextAt(
            ImVec2(area.origin.x + 560.0f, rowY + 4.0f),
            release.manifestDigest.substr(0, 16),
            tokens::ACCENT,
            10.0f);

        rowY += 20.0f;
        design::HorizontalRule(area.origin.x, area.origin.x + area.width, rowY, tokens::BORDER_SUBTLE);
    }
}

void PublishScreen::Draw(
    const ScreenArea& area,
    ApplicationState& state,
    domain::IPublishService* publish,
    domain::ICatalogService* catalog) {

    if (publish == nullptr) {
        const float cursorY = ScreenToolbar::Draw(area, "PUBLISH RELEASE", "publishing unavailable");
        ScreenToolbar::Placeholder(area, cursorY, "no game installation detected");
        return;
    }

    const domain::PublisherState& publisher = publish->Publisher();

    const std::string context = publisher.present
        ? std::format("publishing as {}", publisher.fingerprint)
        : std::string("no signing key yet");

    float cursorY = ScreenToolbar::Draw(area, "PUBLISH RELEASE", context);

    cursorY = DrawStepBar_(area, cursorY, state);

    const std::size_t step = state.PublishStep() < STEP_COUNT ? state.PublishStep() : 0;

    if (step == 0) {
        cursorY = DrawKeyStep_(area, cursorY, publish);
    } else if (step == 1) {
        cursorY = DrawSourceStep_(area, cursorY, state, publish);
    } else {
        cursorY = DrawSignStep_(area, cursorY, state, publish, catalog);
    }

    DrawHistory_(area, cursorY + 8.0f, publish);
}

}
