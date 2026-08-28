#pragma once

#include <imgui.h>

#include <string_view>

namespace wgrd::gui::design {
enum class HeadingLevel {
	Primary
	, Section
	, Sub
	, Minor
};

enum class HeadingTone {
	Accent, Success, Warning
};

enum class ButtonVariant {
	Neutral
	, Accent
	, Success
	, Failure
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
	HeadingTone tone = HeadingTone::Accent
);

float HeadingHeight(HeadingLevel level);

constexpr float FIELD_HEIGHT = 24.0f;
constexpr float BUTTON_HEIGHT = 18.0f;

bool Button(
	ImVec2 topLeft,
	std::string_view label,
	ButtonVariant variant,
	bool filled = false,
	float paddingX = 8.0f,
	bool enabled = true,
	float height = BUTTON_HEIGHT
);

ImVec2 ButtonSize(std::string_view label, float paddingX = 8.0f, float height = BUTTON_HEIGHT);

void Pill(ImVec2 topLeft, std::string_view label, ImU32 color);

ImVec2 PillSize(std::string_view label);

bool Checkbox(ImVec2 topLeft, bool checked);

bool RowHit(ImVec2 topLeft, ImVec2 bottomRight, bool& hovered);

enum class RuleState {
	Idle
	, Failed
	, Weak
	, Passed
};

ImU32 RuleTone(RuleState state);

constexpr float WRAPPED_LINE_STEP = 13.0f;
constexpr std::size_t WRAPPED_LINE_LIMIT = 16;

float WrappedTextAt(ImVec2 topLeft, float width, std::string_view value, ImU32 color, float size);

struct TransferSegments {
	float verified = 0.0f;
	float inFlight = 0.0f;
};

void TransferBar(ImVec2 topLeft, float width, float height, const TransferSegments& segments);

bool RowSecondaryHit(ImVec2 topLeft, ImVec2 bottomRight);

void Shadow(ImVec2 topLeft, ImVec2 bottomRight);

bool PointerInside(ImVec2 topLeft, ImVec2 bottomRight);

void ReserveRegion(ImVec2 topLeft, ImVec2 bottomRight);

void ClearReservedRegion();

void Meter(ImVec2 topLeft, float width, float height, float fraction, ImU32 fill);

bool TextField(ImVec2 topLeft, float width, std::string_view identity, char* buffer, std::size_t capacity);

bool PasswordField(ImVec2 topLeft, float width, std::string_view identity, char* buffer, std::size_t capacity);
}
