#include "gui/screens/TransfersScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"

#include <format>
#include <string>

namespace wgrd::gui {

float TransfersScreen::DrawSwarmCard_(
    const ScreenArea& area,
    float cursorY,
    domain::ISwarmService* swarm) const {

    const float cardWidth = area.width * 0.5f;

    design::HeadingBar(ImVec2(area.origin.x, cursorY), cardWidth, "SWARM", design::HeadingLevel::Minor);
    design::HeadingBar(
        ImVec2(area.origin.x + cardWidth, cursorY),
        cardWidth,
        "LISTENER",
        design::HeadingLevel::Minor);

    const float bodyY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);

    if (swarm == nullptr) {
        design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 6.0f), "offline", tokens::TEXT_DISABLED, 19.0f);
        return bodyY + 52.0f;
    }

    const domain::SwarmStatus& status = swarm->Status();

    design::TextAt(
        ImVec2(area.origin.x + 6.0f, bodyY + 4.0f),
        std::format("{} dht nodes", status.dhtNodes),
        status.dhtRunning ? tokens::ACCENT : tokens::TEXT_DISABLED,
        19.0f);

    design::TextAt(
        ImVec2(area.origin.x + 6.0f, bodyY + 30.0f),
        status.dhtRunning ? "dht running" : "dht starting",
        tokens::SECONDARY_MUTED,
        10.0f);

    design::TextAt(
        ImVec2(area.origin.x + cardWidth + 6.0f, bodyY + 4.0f),
        std::format("port {}", status.listenPort),
        status.running ? tokens::SUCCESS : tokens::FAILURE,
        19.0f);

    design::TextAt(
        ImVec2(area.origin.x + cardWidth + 6.0f, bodyY + 30.0f),
        status.listenInterface,
        tokens::SECONDARY_MUTED,
        10.0f);

    return bodyY + 52.0f;
}

float TransfersScreen::DrawSeeding_(
    const ScreenArea& area,
    float cursorY,
    domain::ISeedingService* seeding) const {

    if (seeding == nullptr) {
        design::HeadingBar(
            ImVec2(area.origin.x, cursorY),
            area.width,
            "SEEDING",
            design::HeadingLevel::Minor);

        const float bodyY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);
        ScreenToolbar::Placeholder(area, bodyY, "seeding unavailable");
        return bodyY + 32.0f;
    }

    const std::vector<domain::SeedEntry>& entries = seeding->Entries();

    design::HeadingBar(
        ImVec2(area.origin.x, cursorY),
        area.width,
        std::format(
            "SEEDING - {} MODS - {} UPLOADED",
            entries.size(),
            ScreenToolbar::FormatBytes(seeding->UploadedBytes())),
        design::HeadingLevel::Minor,
        seeding->Enabled() ? design::HeadingTone::Success : design::HeadingTone::Warning);

    float rowY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);

    if (design::Button(
            ImVec2(area.origin.x + area.width - 70.0f, cursorY + 2.0f),
            seeding->Enabled() ? "pause" : "resume",
            design::ButtonVariant::Neutral)) {
        seeding->SetEnabled(!seeding->Enabled());
    }

    if (entries.empty()) {
        ScreenToolbar::Placeholder(area, rowY, "nothing installed and held yet");
        return rowY + 32.0f;
    }

    for (const domain::SeedEntry& entry : entries) {
        design::Pill(
            ImVec2(area.origin.x + 6.0f, rowY + 4.0f),
            entry.seeding ? "SEEDING" : "STARTING",
            entry.seeding ? tokens::SUCCESS : tokens::ADVISORY);

        design::TextAt(ImVec2(area.origin.x + 96.0f, rowY + 5.0f), entry.modName, tokens::SECONDARY, 11.0f);

        design::TextAt(
            ImVec2(area.origin.x + 300.0f, rowY + 5.0f),
            std::format("v{}", entry.version),
            tokens::SECONDARY_MUTED,
            10.0f);

        design::TextAt(
            ImVec2(area.origin.x + 360.0f, rowY + 5.0f),
            ScreenToolbar::FormatBytes(entry.payloadBytes),
            tokens::SECONDARY,
            10.0f);

        design::TextAt(
            ImVec2(area.origin.x + 460.0f, rowY + 5.0f),
            std::format("{} chunks", entry.chunkCount),
            tokens::SECONDARY_MUTED,
            10.0f);

        design::TextAt(
            ImVec2(area.origin.x + 570.0f, rowY + 5.0f),
            std::format("{} peers", entry.peers),
            entry.peers > 0 ? tokens::ACCENT : tokens::SECONDARY_MUTED,
            10.0f);

        design::TextAt(
            ImVec2(area.origin.x + 660.0f, rowY + 5.0f),
            ScreenToolbar::FormatBytes(entry.uploadedBytes),
            tokens::SECONDARY_MUTED,
            10.0f);

        design::TextAt(
            ImVec2(area.origin.x + 760.0f, rowY + 5.0f),
            entry.infoHash.substr(0, 16),
            tokens::TEXT_DISABLED,
            9.0f);

        rowY += HELD_ROW_HEIGHT;
        design::HorizontalRule(area.origin.x, area.origin.x + area.width, rowY, tokens::BORDER_SUBTLE);
    }

    return rowY;
}

void TransfersScreen::DrawDownloads_(
    const ScreenArea& area,
    float cursorY,
    domain::IInstallService* install) const {

    design::HeadingBar(
        ImVec2(area.origin.x, cursorY),
        area.width,
        "DOWNLOADS",
        design::HeadingLevel::Minor);

    const float bodyY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);

    if (install == nullptr) {
        ScreenToolbar::Placeholder(area, bodyY, "installing unavailable");
        return;
    }

    const domain::InstallProgress progress = install->Progress();

    if (progress.phase == domain::InstallPhase::Idle) {
        ScreenToolbar::Placeholder(area, bodyY, "idle - install a mod from the catalog");
        return;
    }

    design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 5.0f), progress.modName, tokens::SECONDARY, 12.0f);

    const ImU32 tone =
        progress.phase == domain::InstallPhase::Failed ? tokens::FAILURE :
        (progress.phase == domain::InstallPhase::Done ? tokens::SUCCESS : tokens::ACCENT);

    design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 20.0f), progress.message, tone, 10.0f);

    design::TextAt(
        ImVec2(area.origin.x + 300.0f, bodyY + 5.0f),
        std::format("{} to fetch", ScreenToolbar::FormatBytes(progress.remoteBytes)),
        tokens::SECONDARY,
        10.0f);

    design::TextAt(
        ImVec2(area.origin.x + 300.0f, bodyY + 20.0f),
        std::format("{} reused", ScreenToolbar::FormatBytes(progress.heldBytes)),
        tokens::SUCCESS,
        10.0f);

    design::TextAt(
        ImVec2(area.origin.x + 460.0f, bodyY + 5.0f),
        std::format("{} chunks", progress.remoteChunks),
        tokens::SECONDARY_MUTED,
        10.0f);

    design::TextAt(
        ImVec2(area.origin.x + 460.0f, bodyY + 20.0f),
        std::format("{} peers", progress.peers),
        tokens::SECONDARY_MUTED,
        10.0f);

    if (progress.remoteBytes > 0) {
        const float fraction = static_cast<float>(
            static_cast<double>(progress.fetchedBytes) / static_cast<double>(progress.remoteBytes));

        design::Meter(
            ImVec2(area.origin.x + area.width - 220.0f, bodyY + 10.0f),
            210.0f,
            9.0f,
            fraction,
            tokens::ACCENT);
    }

    if (progress.Busy()) {
        if (design::Button(
                ImVec2(area.origin.x + area.width - 70.0f, bodyY + 26.0f),
                "cancel",
                design::ButtonVariant::Failure)) {
            install->Cancel();
        }
    }
}

void TransfersScreen::Draw(
    const ScreenArea& area,
    domain::ISwarmService* swarm,
    domain::ISeedingService* seeding,
    domain::IInstallService* install) {

    float cursorY = ScreenToolbar::Draw(area, "TRANSFERS", "chunk sets served from installed mods");

    cursorY = DrawSwarmCard_(area, cursorY + 4.0f, swarm);
    cursorY = DrawSeeding_(area, cursorY + 8.0f, seeding);

    DrawDownloads_(area, cursorY + 8.0f, install);
}

}
