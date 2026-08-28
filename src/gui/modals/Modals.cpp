#include "gui/modals/Modals.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"

#include <format>
#include <string>

#include <array>

namespace wgrd::gui {

namespace {

constexpr float SETTINGS_WIDTH = 520.0f;
constexpr float DETAIL_WIDTH = 660.0f;
constexpr float MODAL_TITLE_HEIGHT = 22.0f;
constexpr float SETTINGS_HEIGHT = 452.0f;
constexpr float DETAIL_HEIGHT = 300.0f;

bool ModalTitle(ImVec2 origin, float width, std::string_view title, std::string_view meta) {
    const ImVec2 bottomRight(origin.x + width, origin.y + MODAL_TITLE_HEIGHT);

    design::FillRect(origin, bottomRight, tokens::ACCENT);
    design::TextAt(ImVec2(origin.x + 6.0f, origin.y + 4.0f), title, tokens::PRIMARY, 13.0f);

    if (!meta.empty()) {
        const float titleWidth = design::MeasureText(title, 13.0f).x;
        design::TextAt(
            ImVec2(origin.x + 14.0f + titleWidth, origin.y + 6.0f),
            meta,
            tokens::FromHex(0x0E0E12, 0.75f),
            10.0f);
    }

    const ImVec2 closeTopLeft(bottomRight.x - MODAL_TITLE_HEIGHT, origin.y);
    bool hovered = false;
    const bool clicked = design::RowHit(closeTopLeft, bottomRight, hovered);

    if (hovered) {
        design::FillRect(closeTopLeft, bottomRight, tokens::FromHex(0x0E0E12, 0.12f));
    }

    const ImVec2 extent = design::MeasureText("x", 11.0f);
    design::TextAt(
        ImVec2(
            closeTopLeft.x + (MODAL_TITLE_HEIGHT - extent.x) * 0.5f,
            origin.y + (MODAL_TITLE_HEIGHT - extent.y) * 0.5f),
        "x",
        tokens::PRIMARY,
        11.0f);

    return clicked;
}

void ModalShell(ImVec2 origin, float width, float height) {
    design::FillRect(
        ImVec2(origin.x + tokens::SHADOW_OFFSET, origin.y + tokens::SHADOW_OFFSET),
        ImVec2(origin.x + width + tokens::SHADOW_OFFSET, origin.y + height + tokens::SHADOW_OFFSET),
        tokens::FromHex(0x000000, 0.9f));

    design::FillRect(origin, ImVec2(origin.x + width, origin.y + height), tokens::PRIMARY);
    design::StrokeRect(origin, ImVec2(origin.x + width, origin.y + height), tokens::ACCENT);
}

bool ToggleRow(ImVec2 origin, float width, std::string_view label, std::string_view hint, bool value) {
    design::TextAt(ImVec2(origin.x + 6.0f, origin.y + 2.0f), label, tokens::SECONDARY, 11.0f);
    design::TextAt(ImVec2(origin.x + 6.0f, origin.y + 15.0f), hint, tokens::SECONDARY_MUTED, 10.0f);

    const std::string_view text = value ? "on" : "off";
    const ImVec2 size = design::ButtonSize(text);
    const ImVec2 topLeft(origin.x + width - size.x - 6.0f, origin.y + 4.0f);

    return design::Button(
        topLeft,
        text,
        value ? design::ButtonVariant::Success : design::ButtonVariant::Neutral);
}

}

void Modals::Reserve(ImVec2 frameOrigin, float frameWidth, const ApplicationState& state) const {
    if (state.SettingsOpen()) {
        const ImVec2 origin(frameOrigin.x + (frameWidth - SETTINGS_WIDTH) * 0.5f, frameOrigin.y + 70.0f);
        design::ReserveRegion(origin, ImVec2(origin.x + SETTINGS_WIDTH, origin.y + SETTINGS_HEIGHT));
        return;
    }

    if (state.DetailOpen()) {
        const ImVec2 origin(frameOrigin.x + (frameWidth - DETAIL_WIDTH) * 0.5f, frameOrigin.y + 104.0f);
        design::ReserveRegion(origin, ImVec2(origin.x + DETAIL_WIDTH, origin.y + DETAIL_HEIGHT));
        return;
    }

    design::ClearReservedRegion();
}

void Modals::Draw(ImVec2 frameOrigin, float frameWidth, ApplicationState& state, const ApplicationServices& services) {
    design::ClearReservedRegion();

    if (state.DetailOpen()) {
        if (state.SettingsOpen()) {
            const ImVec2 origin(frameOrigin.x + (frameWidth - SETTINGS_WIDTH) * 0.5f, frameOrigin.y + 70.0f);
            design::ReserveRegion(origin, ImVec2(origin.x + SETTINGS_WIDTH, origin.y + SETTINGS_HEIGHT));
        }
        DrawDetail_(frameOrigin, frameWidth, state);
        design::ClearReservedRegion();
    }

    if (state.SettingsOpen()) {
        DrawSettings_(frameOrigin, frameWidth, state, services);
    }

    design::ClearReservedRegion();
}

void Modals::DrawSettings_(ImVec2 frameOrigin, float frameWidth, ApplicationState& state, const ApplicationServices& services) {
    const float height = SETTINGS_HEIGHT;
    const ImVec2 origin(frameOrigin.x + (frameWidth - SETTINGS_WIDTH) * 0.5f, frameOrigin.y + 70.0f);

    ModalShell(origin, SETTINGS_WIDTH, height);

    if (ModalTitle(origin, SETTINGS_WIDTH, "Settings", "")) {
        state.CloseSettings();
    }

    float cursorY = origin.y + MODAL_TITLE_HEIGHT;

    design::HeadingBar(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "GAME LOCATION", design::HeadingLevel::Minor);
    cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

    design::FillRect(
        ImVec2(origin.x + 6.0f, cursorY + 6.0f),
        ImVec2(origin.x + SETTINGS_WIDTH - 70.0f, cursorY + 28.0f),
        tokens::SURFACE_SUNKEN);
    design::StrokeRect(
        ImVec2(origin.x + 6.0f, cursorY + 6.0f),
        ImVec2(origin.x + SETTINGS_WIDTH - 70.0f, cursorY + 28.0f),
        tokens::BORDER);
    design::TextAt(
        ImVec2(origin.x + 12.0f, cursorY + 12.0f),
        "V:/__GAME/__STEAM/steamapps/common/Wargame Red Dragon",
        tokens::SECONDARY,
        10.0f);
    design::Button(ImVec2(origin.x + SETTINGS_WIDTH - 62.0f, cursorY + 7.0f), "browse", design::ButtonVariant::Neutral);
    cursorY += 34.0f;

    design::TextAt(ImVec2(origin.x + 6.0f, cursorY), "detected steam - v.131635", tokens::SUCCESS, 10.0f);
    cursorY += 18.0f;

    design::HeadingBar(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "TRANSFER", design::HeadingLevel::Minor);
    cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

    if (ToggleRow(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "bittorrent", "trackerless - dht pex lan", _bittorrent)) {
        _bittorrent = !_bittorrent;
    }
    cursorY += 28.0f;

    if (ToggleRow(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "fastcdc chunking", "avg 4 MiB - reuse 99.8%", _chunking)) {
        _chunking = !_chunking;
    }
    cursorY += 28.0f;

    if (ToggleRow(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "seed after install", "ratio target 2.0", _seedAfterInstall)) {
        _seedAfterInstall = !_seedAfterInstall;
    }
    cursorY += 28.0f;

    if (ToggleRow(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "seed multiple mods", "off means one mod", _seedMultiple)) {
        _seedMultiple = !_seedMultiple;
    }
    cursorY += 28.0f;

    if (ToggleRow(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "mirror all releases", "seeds every signed release", _mirrorAll)) {
        _mirrorAll = !_mirrorAll;
    }
    cursorY += 28.0f;

    if (ToggleRow(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "https mirrors", "fallback for dead swarm", _httpsMirrors)) {
        _httpsMirrors = !_httpsMirrors;
    }
    cursorY += 30.0f;

    cursorY = DrawTrust_(origin, cursorY, services.registry);

    cursorY = DrawUpdates_(origin, cursorY, state, services.updates);

    if (design::Button(
            ImVec2(origin.x + SETTINGS_WIDTH - 60.0f, origin.y + height - 26.0f),
            "DONE",
            design::ButtonVariant::Accent,
            true,
            12.0f)) {
        state.CloseSettings();
    }
}

void Modals::DrawDetail_(ImVec2 frameOrigin, float frameWidth, ApplicationState& state) {
    const float height = DETAIL_HEIGHT;
    const ImVec2 origin(frameOrigin.x + (frameWidth - DETAIL_WIDTH) * 0.5f, frameOrigin.y + 104.0f);

    ModalShell(origin, DETAIL_WIDTH, height);

    if (ModalTitle(origin, DETAIL_WIDTH, state.DetailTarget(), "1.0.4 - 17 MB - mod - sig ok")) {
        state.CloseDetail();
    }

    float cursorY = origin.y + MODAL_TITLE_HEIGHT + 8.0f;

    design::TextAt(
        ImVec2(origin.x + 8.0f, cursorY),
        "Installs as a folder under Mods/. The manager adds it to the load order",
        tokens::SECONDARY,
        12.0f);
    cursorY += 16.0f;
    design::TextAt(
        ImVec2(origin.x + 8.0f, cursorY),
        "and pins the version. Nothing is merged and nothing is built.",
        tokens::SECONDARY,
        12.0f);
    cursorY += 24.0f;

    design::HeadingBar(ImVec2(origin.x, cursorY), DETAIL_WIDTH, "FOLDER CONTENTS", design::HeadingLevel::Minor);
    cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

    constexpr std::array<const char*, 4> CONTENTS = {
        "131635/Data.dat",
        "131635/NDF_Win.dat",
        "131635/ZZ_1.dat",
        "131635/ZZ_4.dat"
    };

    for (const char* entry : CONTENTS) {
        design::TextAt(ImVec2(origin.x + 8.0f, cursorY + 4.0f), entry, tokens::SECONDARY, 11.0f);
        cursorY += 18.0f;
        design::HorizontalRule(origin.x, origin.x + DETAIL_WIDTH, cursorY, tokens::BORDER_SUBTLE);
    }

    cursorY += 8.0f;
    design::HeadingBar(
        ImVec2(origin.x, cursorY),
        DETAIL_WIDTH,
        "SIGNATURE",
        design::HeadingLevel::Minor,
        design::HeadingTone::Success);
    cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

    design::TextAt(ImVec2(origin.x + 8.0f, cursorY + 4.0f), "verified - ed25519", tokens::SUCCESS, 10.0f);
    design::TextAt(
        ImVec2(origin.x + 8.0f, cursorY + 18.0f),
        "9f2c41aa07be1d33c8e04b71fa9022e5",
        tokens::SECONDARY_MUTED,
        10.0f);

    design::Button(
        ImVec2(origin.x + 8.0f, origin.y + height - 28.0f),
        "ADD TO ORDER",
        design::ButtonVariant::Accent,
        true,
        12.0f);
}


float Modals::DrawUpdates_(ImVec2 origin, float cursorY, ApplicationState& state, domain::IUpdateService* updates) const {
    design::HeadingBar(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "UPDATES", design::HeadingLevel::Minor);
    cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

    if (updates == nullptr) {
        design::TextAt(
            ImVec2(origin.x + 6.0f, cursorY + 6.0f),
            "updates unavailable",
            tokens::TEXT_DISABLED,
            10.0f);
        return cursorY + 30.0f;
    }

    const domain::UpdateStatus status = updates->Status();

    const std::string versions = status.latestVersion.empty()
        ? std::format("installed {}", status.currentVersion)
        : std::format("installed {} - latest {}", status.currentVersion, status.latestVersion);

    design::TextAt(ImVec2(origin.x + 6.0f, cursorY + 6.0f), versions, tokens::SECONDARY, 10.0f);

    const ImU32 messageColor =
        status.phase == domain::UpdatePhase::Failed ? tokens::FAILURE :
        (status.phase == domain::UpdatePhase::Ready ? tokens::SUCCESS :
        (status.phase == domain::UpdatePhase::Available ? tokens::ACCENT : tokens::SECONDARY_MUTED));

    design::TextAt(ImVec2(origin.x + 6.0f, cursorY + 20.0f), status.message, messageColor, 10.0f);

    if (status.phase == domain::UpdatePhase::Downloading) {
        const float fraction = status.totalBytes == 0
            ? 0.0f
            : static_cast<float>(
                static_cast<double>(status.downloadedBytes) / static_cast<double>(status.totalBytes));

        design::Meter(
            ImVec2(origin.x + SETTINGS_WIDTH - 200.0f, cursorY + 12.0f),
            190.0f,
            9.0f,
            fraction,
            tokens::ACCENT);

        design::TextAt(
            ImVec2(origin.x + SETTINGS_WIDTH - 200.0f, cursorY + 24.0f),
            std::format(
                "{:.1f} of {:.1f} MiB",
                static_cast<double>(status.downloadedBytes) / 1048576.0,
                static_cast<double>(status.totalBytes) / 1048576.0),
            tokens::SECONDARY_MUTED,
            9.0f);

        return cursorY + 44.0f;
    }

    if (status.Busy()) {
        return cursorY + 40.0f;
    }

    if (status.phase == domain::UpdatePhase::Available) {
        if (design::Button(
                ImVec2(origin.x + SETTINGS_WIDTH - 180.0f, cursorY + 8.0f),
                "DOWNLOAD UPDATE",
                design::ButtonVariant::Accent,
                true)) {
            updates->Download();
        }
        return cursorY + 40.0f;
    }

    if (status.phase == domain::UpdatePhase::Ready) {
        if (design::Button(
                ImVec2(origin.x + SETTINGS_WIDTH - 190.0f, cursorY + 8.0f),
                "RESTART AND UPDATE",
                design::ButtonVariant::Success,
                true)) {
            if (updates->ApplyAndRestart()) {
                state.RequestExit();
            }
        }
        return cursorY + 40.0f;
    }

    if (design::Button(
            ImVec2(origin.x + SETTINGS_WIDTH - 170.0f, cursorY + 8.0f),
            "CHECK FOR UPDATES",
            design::ButtonVariant::Neutral)) {
        updates->Check();
    }

    return cursorY + 40.0f;
}


float Modals::DrawTrust_(ImVec2 origin, float cursorY, domain::IRegistryUpdater* updater) const {
    design::HeadingBar(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, "TRUST REGISTRY", design::HeadingLevel::Minor);
    cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

    if (updater == nullptr) {
        design::TextAt(
            ImVec2(origin.x + 6.0f, cursorY + 6.0f),
            "registry unavailable",
            tokens::TEXT_DISABLED,
            10.0f);
        return cursorY + 30.0f;
    }

    const domain::RegistryStatus status = updater->Status();

    design::TextAt(
        ImVec2(origin.x + 6.0f, cursorY + 6.0f),
        std::format("{} keys trusted", status.keyCount),
        tokens::SECONDARY,
        10.0f);

    const ImU32 messageColor =
        status.phase == domain::RegistryPhase::Failed ? tokens::FAILURE :
        (status.phase == domain::RegistryPhase::Fresh ? tokens::SUCCESS : tokens::ADVISORY);

    const std::string detail = status.phase == domain::RegistryPhase::Fresh
        ? std::format("{} - synced {}s ago", status.message, status.secondsSincePoll)
        : status.message;

    design::TextAt(ImVec2(origin.x + 6.0f, cursorY + 20.0f), detail, messageColor, 10.0f);

    if (!status.Busy()) {
        if (design::Button(
                ImVec2(origin.x + SETTINGS_WIDTH - 150.0f, cursorY + 8.0f),
                "SYNC REGISTRY",
                design::ButtonVariant::Neutral)) {
            updater->Poll();
        }
    }

    return cursorY + 40.0f;
}

}