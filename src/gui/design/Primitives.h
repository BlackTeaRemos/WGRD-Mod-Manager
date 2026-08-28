#pragma once

#include <imgui.h>

#include <cstddef>
#include <string_view>

namespace wgrd::gui::design {

enum class HeadingLevel {
    Primary,
    Section,
    Sub,
    Minor
};

enum class HeadingTone {
    Accent,
    Success,
    Warning
};

enum class ButtonVariant {
    Neutral,
    Accent,
    Success,
    Failure
};

float ScaledText(float pixels);

void Text(std::string_view value, ImU32 color, float size);

void TextAt(ImVec2 position, std::string_view value, ImU32 color, float size);

ImVec2 MeasureText(std::string_view value, float size);

void FillRect(ImVec2 topLeft, ImVec2 bottomRight, ImU32 color);

void StrokeRect(ImVec2 topLeft, ImVec2 bottomRight, ImU32 color);

void HorizontalRule(float left, float right, float y, ImU32 color);

void HeadingBar(
    ImVec2 topLeft,
    float width,
    std::string_view label,
    HeadingLevel level,
    HeadingTone tone = HeadingTone::Accent);

float HeadingHeight(HeadingLevel level);

bool Button(
    ImVec2 topLeft,
    std::string_view label,
    ButtonVariant variant,
    bool filled = false,
    float paddingX = 8.0f);

ImVec2 ButtonSize(std::string_view label, float paddingX = 8.0f);

void Pill(ImVec2 topLeft, std::string_view label, ImU32 color);

ImVec2 PillSize(std::string_view label);

bool Checkbox(ImVec2 topLeft, bool checked);

bool RowHit(ImVec2 topLeft, ImVec2 bottomRight, bool& hovered);

bool PointerInside(ImVec2 topLeft, ImVec2 bottomRight);

void ReserveRegion(ImVec2 topLeft, ImVec2 bottomRight);

void ClearReservedRegion();

void Meter(ImVec2 topLeft, float width, float height, float fraction, ImU32 fill);

bool TextField(ImVec2 topLeft, float width, std::string_view identity, char* buffer, std::size_t capacity);

}
