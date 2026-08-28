#include "gui/design/Primitives.h"

#include "gui/design/Tokens.h"

#include <string>

namespace wgrd::gui::design {

namespace {

constexpr float BASE_FONT_SIZE = 13.0f;
constexpr float CHECKBOX_SIZE = 13.0f;

bool reservedActive = false;
ImVec2 reservedTopLeft{};
ImVec2 reservedBottomRight{};

bool ReservedRegionCovers(ImVec2 pointer) {
    return reservedActive &&
        pointer.x >= reservedTopLeft.x && pointer.x < reservedBottomRight.x &&
        pointer.y >= reservedTopLeft.y && pointer.y < reservedBottomRight.y;
}

ImDrawList* Canvas() {
    return ImGui::GetWindowDrawList();
}

ImU32 HeadingFill(HeadingLevel level, HeadingTone tone) {
    if (tone == HeadingTone::Success) {
        return tokens::SUCCESS_HEADING;
    }
    if (tone == HeadingTone::Warning) {
        return tokens::WARNING_HEADING;
    }

    switch (level) {
        case HeadingLevel::Primary:
            return tokens::ACCENT;
        case HeadingLevel::Section:
            return tokens::ACCENT_HEADING_SECTION;
        case HeadingLevel::Sub:
            return tokens::ACCENT_HEADING_SUB;
        case HeadingLevel::Minor:
            return tokens::ACCENT_HEADING_MINOR;
    }

    return tokens::ACCENT;
}

float HeadingFontSize(HeadingLevel level) {
    switch (level) {
        case HeadingLevel::Primary:
            return 13.0f;
        case HeadingLevel::Section:
            return 12.0f;
        case HeadingLevel::Sub:
            return 11.0f;
        case HeadingLevel::Minor:
            return 10.0f;
    }

    return 11.0f;
}

ImU32 VariantColor(ButtonVariant variant) {
    switch (variant) {
        case ButtonVariant::Accent:
            return tokens::ACCENT;
        case ButtonVariant::Success:
            return tokens::SUCCESS;
        case ButtonVariant::Failure:
            return tokens::FAILURE;
        case ButtonVariant::Neutral:
            return tokens::SECONDARY_MUTED;
    }

    return tokens::SECONDARY_MUTED;
}

ImU32 VariantBorder(ButtonVariant variant) {
    return variant == ButtonVariant::Neutral ? tokens::BORDER : VariantColor(variant);
}

}

float ScaledText(float pixels) {
    return pixels;
}

ImVec2 MeasureText(std::string_view value, float size) {
    const std::string text(value);
    return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
}

void TextAt(ImVec2 position, std::string_view value, ImU32 color, float size) {
    const std::string text(value);
    Canvas()->AddText(ImGui::GetFont(), size, position, color, text.c_str());
}

void Text(std::string_view value, ImU32 color, float size) {
    TextAt(ImGui::GetCursorScreenPos(), value, color, size);
}

void FillRect(ImVec2 topLeft, ImVec2 bottomRight, ImU32 color) {
    Canvas()->AddRectFilled(topLeft, bottomRight, color);
}

void StrokeRect(ImVec2 topLeft, ImVec2 bottomRight, ImU32 color) {
    Canvas()->AddRect(topLeft, bottomRight, color, 0.0f, 0, 1.0f);
}

void HorizontalRule(float left, float right, float y, ImU32 color) {
    Canvas()->AddLine(ImVec2(left, y), ImVec2(right, y), color, 1.0f);
}

float HeadingHeight(HeadingLevel level) {
    switch (level) {
        case HeadingLevel::Primary:
            return tokens::HEADING_PRIMARY_HEIGHT;
        case HeadingLevel::Section:
            return tokens::HEADING_SECTION_HEIGHT;
        case HeadingLevel::Sub:
            return tokens::HEADING_SUB_HEIGHT;
        case HeadingLevel::Minor:
            return tokens::HEADING_MINOR_HEIGHT;
    }

    return tokens::HEADING_MINOR_HEIGHT;
}

void HeadingBar(
    ImVec2 topLeft,
    float width,
    std::string_view label,
    HeadingLevel level,
    HeadingTone tone) {

    const float height = HeadingHeight(level);
    const ImVec2 bottomRight(topLeft.x + width, topLeft.y + height);

    FillRect(topLeft, bottomRight, HeadingFill(level, tone));

    const float fontSize = HeadingFontSize(level);
    const ImVec2 extent = MeasureText(label, fontSize);
    TextAt(
        ImVec2(topLeft.x + 6.0f, topLeft.y + (height - extent.y) * 0.5f),
        label,
        tokens::PRIMARY,
        fontSize);
}

ImVec2 ButtonSize(std::string_view label, float paddingX) {
    const ImVec2 extent = MeasureText(label, 10.0f);
    return ImVec2(extent.x + paddingX * 2.0f, 18.0f);
}

bool Button(
    ImVec2 topLeft,
    std::string_view label,
    ButtonVariant variant,
    bool filled,
    float paddingX) {

    const ImVec2 size = ButtonSize(label, paddingX);
    const ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);

    bool hovered = false;
    const bool clicked = RowHit(topLeft, bottomRight, hovered);

    if (filled || hovered) {
        const ImU32 fill = variant == ButtonVariant::Neutral
            ? tokens::ACCENT_HOVER_FILL
            : (hovered ? tokens::ACCENT_BUTTON_HOVER : tokens::ACCENT_BUTTON_FILL);
        FillRect(topLeft, bottomRight, fill);
    }

    const ImU32 outline = hovered ? tokens::ACCENT_HOVER : VariantBorder(variant);
    const ImU32 textColor = hovered ? tokens::ACCENT_HOVER : VariantColor(variant);

    StrokeRect(topLeft, bottomRight, outline);

    const ImVec2 extent = MeasureText(label, 10.0f);
    TextAt(
        ImVec2(topLeft.x + paddingX, topLeft.y + (size.y - extent.y) * 0.5f),
        label,
        textColor,
        10.0f);

    return clicked;
}

ImVec2 PillSize(std::string_view label) {
    const ImVec2 extent = MeasureText(label, 9.0f);
    return ImVec2(extent.x + 10.0f, extent.y + 4.0f);
}

void Pill(ImVec2 topLeft, std::string_view label, ImU32 color) {
    const ImVec2 size = PillSize(label);
    const ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);

    StrokeRect(topLeft, bottomRight, color);
    TextAt(ImVec2(topLeft.x + 5.0f, topLeft.y + 2.0f), label, color, 9.0f);
}

bool Checkbox(ImVec2 topLeft, bool checked) {
    const ImVec2 bottomRight(topLeft.x + CHECKBOX_SIZE, topLeft.y + CHECKBOX_SIZE);

    bool hovered = false;
    const bool clicked = RowHit(topLeft, bottomRight, hovered);

    StrokeRect(topLeft, bottomRight, checked ? tokens::SUCCESS : tokens::BORDER);

    if (checked) {
        const ImVec2 extent = MeasureText("x", 9.0f);
        TextAt(
            ImVec2(
                topLeft.x + (CHECKBOX_SIZE - extent.x) * 0.5f,
                topLeft.y + (CHECKBOX_SIZE - extent.y) * 0.5f),
            "x",
            tokens::SUCCESS,
            9.0f);
    }

    return clicked;
}

bool RowHit(ImVec2 topLeft, ImVec2 bottomRight, bool& hovered) {
    const ImVec2 pointer = ImGui::GetIO().MousePos;

    const bool inside =
        pointer.x >= topLeft.x && pointer.x < bottomRight.x &&
        pointer.y >= topLeft.y && pointer.y < bottomRight.y;

    if (inside && ReservedRegionCovers(pointer)) {
        hovered = false;
        return false;
    }

    hovered = inside;
    return hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

bool PointerInside(ImVec2 topLeft, ImVec2 bottomRight) {
    const ImVec2 pointer = ImGui::GetIO().MousePos;

    return pointer.x >= topLeft.x && pointer.x < bottomRight.x &&
        pointer.y >= topLeft.y && pointer.y < bottomRight.y;
}

void ReserveRegion(ImVec2 topLeft, ImVec2 bottomRight) {
    reservedActive = true;
    reservedTopLeft = topLeft;
    reservedBottomRight = bottomRight;
}

void ClearReservedRegion() {
    reservedActive = false;
}

void Meter(ImVec2 topLeft, float width, float height, float fraction, ImU32 fill) {
    const ImVec2 bottomRight(topLeft.x + width, topLeft.y + height);

    FillRect(topLeft, bottomRight, tokens::METER_TRACK);
    StrokeRect(topLeft, bottomRight, tokens::BORDER);

    const float clamped = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
    FillRect(topLeft, ImVec2(topLeft.x + width * clamped, bottomRight.y), fill);
}


bool TextField(ImVec2 topLeft, float width, std::string_view identity, char* buffer, std::size_t capacity) {
    constexpr float FIELD_HEIGHT = 24.0f;

    const ImVec2 bottomRight(topLeft.x + width, topLeft.y + FIELD_HEIGHT);

    FillRect(topLeft, bottomRight, tokens::SURFACE_SUNKEN);
    StrokeRect(topLeft, bottomRight, tokens::BORDER);

    ImGui::SetCursorScreenPos(ImVec2(topLeft.x + 6.0f, topLeft.y + 4.0f));
    ImGui::PushItemWidth(width - 12.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, tokens::SECONDARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

    const std::string label = "##" + std::string(identity);
    const bool changed = ImGui::InputText(label.c_str(), buffer, capacity);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    return changed;
}

}