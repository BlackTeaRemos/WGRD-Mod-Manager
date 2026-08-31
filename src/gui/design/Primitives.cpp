#include "gui/design/Primitives.h"

#include "gui/design/Tokens.h"

#include <algorithm>
#include <string>

namespace wgrd::gui::design {
namespace {
	constexpr float CHECKBOX_SIZE = 13.0f;

	bool reservedActive = false;
	ImVec2 reservedTopLeft{};
	ImVec2 reservedBottomRight{};

	bool ReservedRegionCovers(const ImVec2 pointer) {
		return reservedActive &&
		       pointer.x >= reservedTopLeft.x && pointer.x < reservedBottomRight.x &&
		       pointer.y >= reservedTopLeft.y && pointer.y < reservedBottomRight.y;
	}

	ImDrawList* Canvas() {
		return ImGui::GetWindowDrawList();
	}

	ImU32 HeadingFill(const HeadingLevel level, const HeadingTone tone) {
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

	float HeadingFontSize(const HeadingLevel level) {
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

	ImU32 VariantColor(const ButtonVariant variant) {
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

	ImU32 VariantBorder(const ButtonVariant variant) {
		return variant == ButtonVariant::Neutral ? tokens::BORDER : VariantColor(variant);
	}
}

float ScaledText(const float pixels) {
	return pixels;
}

ImVec2 MeasureText(const std::string_view value, const float size) {
	const std::string text(value);
	return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
}

void TextAt(const ImVec2 position, const std::string_view value, const ImU32 color, const float size) {
	const std::string text(value);
	Canvas()->AddText(ImGui::GetFont(), size, position, color, text.c_str());
}

void Text(const std::string_view value, const ImU32 color, const float size) {
	TextAt(ImGui::GetCursorScreenPos(), value, color, size);
}

void FillRect(const ImVec2 topLeft, const ImVec2 bottomRight, const ImU32 color) {
	Canvas()->AddRectFilled(topLeft, bottomRight, color);
}

void StrokeRect(const ImVec2 topLeft, const ImVec2 bottomRight, const ImU32 color) {
	Canvas()->AddRect(topLeft, bottomRight, color, 0.0f, 0, 1.0f);
}

void HorizontalRule(const float left, const float right, const float y, const ImU32 color) {
	Canvas()->AddLine(ImVec2(left, y), ImVec2(right, y), color, 1.0f);
}

float HeadingHeight(const HeadingLevel level) {
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
	const ImVec2 topLeft,
	const float width,
	const std::string_view label,
	const HeadingLevel level,
	const HeadingTone tone
) {
	const float height = HeadingHeight(level);
	const ImVec2 bottomRight(topLeft.x + width, topLeft.y + height);

	FillRect(topLeft, bottomRight, HeadingFill(level, tone));

	const float fontSize = HeadingFontSize(level);
	const ImVec2 extent = MeasureText(label, fontSize);
	TextAt(
		ImVec2(topLeft.x + 6.0f, topLeft.y + (height - extent.y) * 0.5f),
		label,
		tokens::PRIMARY,
		fontSize
	);
}

ImU32 RuleTone(const RuleState state) {
	switch (state) {
		case RuleState::Failed:
			return tokens::FAILURE;
		case RuleState::Weak:
			return tokens::ADVISORY;
		case RuleState::Passed:
			return tokens::SUCCESS;
		case RuleState::Idle:
			break;
	}

	return tokens::TEXT_DISABLED;
}

float WrappedTextAt(
	const ImVec2 topLeft,
	const float width,
	const std::string_view value,
	const ImU32 color,
	const float size
) {
	float cursorY = topLeft.y;
	std::size_t drawn = 0;
	std::string line;
	std::size_t cursor = 0;

	while (cursor <= value.size() && drawn < WRAPPED_LINE_LIMIT) {
		const std::size_t space = value.find(' ', cursor);
		const std::size_t end = space == std::string_view::npos ? value.size() : space;

		const std::string_view word = value.substr(cursor, end - cursor);

		std::string candidate = line.empty()
		                        ? std::string(word)
		                        : line + " " + std::string(word);

		if (!line.empty() && MeasureText(candidate, size).x > width) {
			TextAt(ImVec2(topLeft.x, cursorY), line, color, size);
			cursorY += WRAPPED_LINE_STEP;
			++drawn;
			line = std::string(word);
		} else {
			line = std::move(candidate);
		}

		if (space == std::string_view::npos) {
			break;
		}

		cursor = space + 1;
	}

	if (!line.empty() && drawn < WRAPPED_LINE_LIMIT) {
		TextAt(ImVec2(topLeft.x, cursorY), line, color, size);
		cursorY += WRAPPED_LINE_STEP;
	}

	return cursorY;
}

ImVec2 ButtonSize(const std::string_view label, const float paddingX, const float height) {
	const ImVec2 extent = MeasureText(label, 10.0f);
	return ImVec2(extent.x + paddingX * 2.0f, height);
}

bool Button(
	const ImVec2 topLeft,
	const std::string_view label,
	const ButtonVariant variant,
	const bool filled,
	const float paddingX,
	const bool enabled,
	const float height
) {
	const ImVec2 size = ButtonSize(label, paddingX, height);
	const ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);

	if (!enabled) {
		StrokeRect(topLeft, bottomRight, tokens::BORDER_SUBTLE);

		const ImVec2 mutedExtent = MeasureText(label, 10.0f);
		TextAt(
			ImVec2(topLeft.x + paddingX, topLeft.y + (size.y - mutedExtent.y) * 0.5f),
			label,
			tokens::TEXT_DISABLED,
			10.0f
		);

		return false;
	}

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
		10.0f
	);

	return clicked;
}

ImVec2 PillSize(const std::string_view label) {
	const ImVec2 extent = MeasureText(label, 9.0f);
	return ImVec2(extent.x + 10.0f, extent.y + 4.0f);
}

void Pill(const ImVec2 topLeft, const std::string_view label, const ImU32 color) {
	const ImVec2 size = PillSize(label);
	const ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);

	StrokeRect(topLeft, bottomRight, color);
	TextAt(ImVec2(topLeft.x + 5.0f, topLeft.y + 2.0f), label, color, 9.0f);
}

bool Checkbox(const ImVec2 topLeft, const bool checked) {
	const ImVec2 bottomRight(topLeft.x + CHECKBOX_SIZE, topLeft.y + CHECKBOX_SIZE);

	bool hovered = false;
	const bool clicked = RowHit(topLeft, bottomRight, hovered);

	StrokeRect(topLeft, bottomRight, checked ? tokens::SUCCESS : tokens::BORDER);

	if (checked) {
		const ImVec2 extent = MeasureText("x", 9.0f);
		TextAt(
			ImVec2(
				topLeft.x + (CHECKBOX_SIZE - extent.x) * 0.5f,
				topLeft.y + (CHECKBOX_SIZE - extent.y) * 0.5f
			),
			"x",
			tokens::SUCCESS,
			9.0f
		);
	}

	return clicked;
}

bool RowHit(const ImVec2 topLeft, const ImVec2 bottomRight, bool& hovered) {
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

void TransferBar(const ImVec2 topLeft, const float width, const float height, const TransferSegments& segments) {
	const ImVec2 bottomRight(topLeft.x + width, topLeft.y + height);

	FillRect(topLeft, bottomRight, tokens::SURFACE_SUNKEN);

	const float inner = width - 2.0f;
	float cursor = topLeft.x + 1.0f;

	const float verifiedWidth = inner * std::clamp(segments.verified, 0.0f, 1.0f);
	if (verifiedWidth > 0.0f) {
		FillRect(
			ImVec2(cursor, topLeft.y + 1.0f),
			ImVec2(cursor + verifiedWidth, bottomRight.y - 1.0f),
			tokens::SUCCESS
		);
		cursor += verifiedWidth;
	}

	const float inFlightWidth = inner * std::clamp(segments.inFlight, 0.0f, 1.0f);
	const float inFlightEnd = std::min(cursor + inFlightWidth, bottomRight.x - 1.0f);

	for (float stripe = cursor; stripe < inFlightEnd; stripe += tokens::STRIPE_PERIOD) {
		const float stripeEnd = std::min(stripe + tokens::STRIPE_WIDTH, inFlightEnd);

		FillRect(
			ImVec2(stripe, topLeft.y + 1.0f),
			ImVec2(stripeEnd, bottomRight.y - 1.0f),
			tokens::ACCENT
		);
	}

	StrokeRect(topLeft, bottomRight, tokens::BORDER);
}

bool RowSecondaryHit(const ImVec2 topLeft, const ImVec2 bottomRight) {
	const ImVec2 pointer = ImGui::GetIO().MousePos;

	const bool inside =
			pointer.x >= topLeft.x && pointer.x < bottomRight.x &&
			pointer.y >= topLeft.y && pointer.y < bottomRight.y;

	if (!inside || ReservedRegionCovers(pointer)) {
		return false;
	}

	return ImGui::IsMouseClicked(ImGuiMouseButton_Right);
}

void Shadow(const ImVec2 topLeft, const ImVec2 bottomRight) {
	Canvas()->AddRectFilled(
		ImVec2(topLeft.x + tokens::SHADOW_OFFSET, topLeft.y + tokens::SHADOW_OFFSET),
		ImVec2(bottomRight.x + tokens::SHADOW_OFFSET, bottomRight.y + tokens::SHADOW_OFFSET),
		tokens::SHADOW
	);
}

bool PointerInside(const ImVec2 topLeft, const ImVec2 bottomRight) {
	const ImVec2 pointer = ImGui::GetIO().MousePos;

	return pointer.x >= topLeft.x && pointer.x < bottomRight.x &&
	       pointer.y >= topLeft.y && pointer.y < bottomRight.y;
}

void ReserveRegion(const ImVec2 topLeft, const ImVec2 bottomRight) {
	reservedActive = true;
	reservedTopLeft = topLeft;
	reservedBottomRight = bottomRight;
}

void ClearReservedRegion() {
	reservedActive = false;
}

void Meter(const ImVec2 topLeft, const float width, const float height, const float fraction, const ImU32 fill) {
	const ImVec2 bottomRight(topLeft.x + width, topLeft.y + height);

	FillRect(topLeft, bottomRight, tokens::METER_TRACK);
	StrokeRect(topLeft, bottomRight, tokens::BORDER);

	const float clamped = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
	FillRect(topLeft, ImVec2(topLeft.x + width * clamped, bottomRight.y), fill);
}


namespace {
	bool DrawInputField(
		const ImVec2 topLeft,
		const float width,
		const std::string_view identity,
		char* buffer,
		const std::size_t capacity,
		const ImGuiInputTextFlags flags
	) {
		const ImVec2 bottomRight(topLeft.x + width, topLeft.y + FIELD_HEIGHT);

		FillRect(topLeft, bottomRight, tokens::SURFACE_SUNKEN);
		StrokeRect(topLeft, bottomRight, tokens::BORDER);

		ImGui::SetCursorScreenPos(ImVec2(topLeft.x + 6.0f, topLeft.y + 4.0f));
		ImGui::PushItemWidth(width - 12.0f);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, tokens::SECONDARY);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

		const std::string label = "##" + std::string(identity);
		const bool changed = ImGui::InputText(label.c_str(), buffer, capacity, flags);

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
		ImGui::PopItemWidth();

		return changed;
	}
}

bool TextField(const ImVec2 topLeft, const float width, const std::string_view identity, char* buffer, const std::size_t capacity) {
	return DrawInputField(topLeft, width, identity, buffer, capacity, ImGuiInputTextFlags_None);
}

bool PasswordField(const ImVec2 topLeft, const float width, const std::string_view identity, char* buffer, const std::size_t capacity) {
	return DrawInputField(topLeft, width, identity, buffer, capacity, ImGuiInputTextFlags_Password);
}
}
