#pragma once

#include <imgui.h>

#include <cstdint>

namespace wgrd::gui::tokens {

constexpr ImU32 FromHex(std::uint32_t rgb, float alpha = 1.0f) {
    const ImU32 red = (rgb >> 16) & 0xFF;
    const ImU32 green = (rgb >> 8) & 0xFF;
    const ImU32 blue = rgb & 0xFF;
    const ImU32 opacity = static_cast<ImU32>(alpha * 255.0f + 0.5f);
    return (opacity << IM_COL32_A_SHIFT) | (blue << IM_COL32_B_SHIFT) |
           (green << IM_COL32_G_SHIFT) | (red << IM_COL32_R_SHIFT);
}

constexpr ImU32 PAGE_BACKGROUND = FromHex(0x07070A);
constexpr ImU32 PRIMARY = FromHex(0x0E0E12);
constexpr ImU32 SURFACE_RAISED = FromHex(0x12121A);
constexpr ImU32 SURFACE_SUNKEN = FromHex(0x0B0B0F);
constexpr ImU32 SECONDARY = FromHex(0xC8C8D0);
constexpr ImU32 SECONDARY_MUTED = FromHex(0x808090);
constexpr ImU32 TEXT_DISABLED = FromHex(0x4A4A58);
constexpr ImU32 BORDER = FromHex(0x2A2A38);
constexpr ImU32 BORDER_SUBTLE = FromHex(0x1A1A24);
constexpr ImU32 ACCENT = FromHex(0xE07830);
constexpr ImU32 ACCENT_HOVER = FromHex(0xF09040);
constexpr ImU32 SUCCESS = FromHex(0x40B868);
constexpr ImU32 FAILURE = FromHex(0xD04040);
constexpr ImU32 ADVISORY = FromHex(0xE0C030);
constexpr ImU32 EXTENSION_KIND = FromHex(0xC792EA);
constexpr ImU32 TRANSPORT_HTTPS = FromHex(0x82AAFF);
constexpr ImU32 METER_TRACK = FromHex(0x1C1C20);

constexpr ImU32 ACCENT_WARNED_ROW = FromHex(0xE07830, 0.04f);
constexpr ImU32 ACCENT_QUEUED_ROW = FromHex(0xE07830, 0.06f);
constexpr ImU32 ACCENT_HOVER_FILL = FromHex(0xE07830, 0.09f);
constexpr ImU32 ACCENT_BUTTON_FILL = FromHex(0xE07830, 0.14f);
constexpr ImU32 ACCENT_ACTIVE_FILL = FromHex(0xE07830, 0.18f);
constexpr ImU32 ACCENT_RAIL_HEADER = FromHex(0xE07830, 0.25f);
constexpr ImU32 ACCENT_BUTTON_HOVER = FromHex(0xE07830, 0.26f);
constexpr ImU32 ACCENT_HEADING_MINOR = FromHex(0xE07830, 0.40f);
constexpr ImU32 ACCENT_HEADING_SUB = FromHex(0xE07830, 0.55f);
constexpr ImU32 ACCENT_HEADING_SECTION = FromHex(0xE07830, 0.70f);
constexpr ImU32 SUCCESS_HEADING = FromHex(0x40B868, 0.50f);
constexpr ImU32 WARNING_HEADING = FromHex(0xE0C030, 0.80f);

constexpr float FRAME_WIDTH = 1320.0f;
constexpr float FRAME_MIN_HEIGHT = 770.0f;
constexpr float TITLE_BAR_HEIGHT = 26.0f;
constexpr float STATUS_BAR_HEIGHT = 20.0f;
constexpr float RAIL_WIDTH = 186.0f;
constexpr float RAIL_HEADER_HEIGHT = 15.0f;

constexpr float HEADING_PRIMARY_HEIGHT = 22.0f;
constexpr float HEADING_SECTION_HEIGHT = 19.0f;
constexpr float HEADING_SUB_HEIGHT = 17.0f;
constexpr float HEADING_MINOR_HEIGHT = 16.0f;

constexpr float SHADOW_OFFSET = 4.0f;

}
