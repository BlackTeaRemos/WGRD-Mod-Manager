#include "gui/screens/order/OrderEntryList.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/text/ClippedText.h"
#include "gui/text/CommonText.h"
#include "gui/text/OrderText.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>

namespace wgrd::gui {
const domain::Annotation* OrderEntryList::AnnotationFor_(
	const domain::OrderSnapshot& snapshot,
	const domain::InstallFolder& folder
) {
	for (const domain::Annotation& annotation : snapshot.annotations) {
		if (annotation.folder == folder) {
			return &annotation;
		}
	}

	return nullptr;
}

bool OrderEntryList::Unverified_(
	domain::ICatalogService* catalogService,
	const domain::InstallFolder& folder
) {
	if (catalogService == nullptr) {
		return false;
	}

	return !catalogService->Verified(folder.Value());
}

bool OrderEntryList::Draw(
	const ScreenArea& area,
	const float cursorY,
	const float height,
	const float rightX,
	const domain::OrderSnapshot& snapshot,
	domain::IOrderService* orderService,
	domain::ICatalogService* catalogService
) {
	_drag.Begin();
	std::optional<float> insertionY;
	const float leftWidth = rightX - area.origin.x;
	const float listHeight = std::max(height - (cursorY - area.origin.y), 1.0f);
	bool apply = false;

	ImGui::SetCursorScreenPos(ImVec2(area.origin.x, cursorY));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild(
		"order_entries",
		ImVec2(leftWidth, listHeight),
		ImGuiChildFlags_None,
		ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoSavedSettings
	);
	float leftY = cursorY;

	design::HeadingBar(
		ImVec2(area.origin.x, leftY),
		leftWidth,
		text::order::HEADING,
		design::HeadingLevel::Section
	);

	leftY += design::HeadingHeight(design::HeadingLevel::Section);

	std::size_t index = 0;
	for (const domain::OrderEntryView& entry : snapshot.entries) {
		ImGui::PushID(entry.folder.Value().c_str());
		const ImVec2 rowTopLeft(area.origin.x, leftY);
		const float rowHeight = ROW_HEIGHT + (entry.enabled && !entry.present ? 14.0f : 0.0f);
		const ImVec2 rowBottomRight(rightX, leftY + rowHeight);

		const bool blocking = entry.enabled && !entry.present;
		const bool selected = entry.folder.Value() == _selected;

		bool hovered = false;
		const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered, true);
		const bool focused = ImGui::IsItemFocused();
		const auto insertion = _drag.Row(rowTopLeft, rowBottomRight, entry.folder, index);
		if (insertion.has_value()) {
			insertionY = insertion;
		}

		if (blocking) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_WARNED_ROW);
			design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::FAILURE);
		} else if (selected) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_ACTIVE_FILL);
		} else if (hovered) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
		}

		if (selected) {
			design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::ACCENT);
		}

		if (clicked || focused) {
			_selected = entry.folder.Value();
		}

		const std::string position = std::format(text::common::POSITION_FORMAT, index + 1);

		design::TextAt(ImVec2(rowTopLeft.x + 8.0f, leftY + 8.0f), text::order::DRAG_HANDLE, tokens::TEXT_DISABLED, 11.0f);
		design::TextAt(ImVec2(rowTopLeft.x + 22.0f, leftY + 8.0f), position, tokens::TEXT_DISABLED, 11.0f);

		if (design::Checkbox(ImVec2(rowTopLeft.x + 42.0f, leftY + 6.0f), entry.enabled)) {
			orderService->SetEnabled(entry.folder, !entry.enabled);
			apply = true;
			ImGui::PopID();
			break;
		}

		if (ImGui::IsItemFocused()) {
			_selected = entry.folder.Value();
		}
		const ImU32 nameColor = entry.enabled ? tokens::SECONDARY : tokens::TEXT_DISABLED;
		design::ClippedText::Draw(ImVec2(rowTopLeft.x + 62.0f, leftY + 4.0f), leftWidth - 125.0f, entry.folder.Value(), nameColor, 12.0f, FontRole::Name);

		if (Unverified_(catalogService, entry.folder) && leftWidth > 600.0f) {
			const float nameWidth = design::MeasureText(entry.folder.Value(), 12.0f, FontRole::Name).x;

			design::TextAt(
				ImVec2(rowTopLeft.x + 68.0f + nameWidth, leftY + 6.0f),
				text::order::UNSIGNED,
				tokens::FAILURE,
				9.0f
			);
		}

		const std::string location = snapshot.modsDirectory.empty()
		                             ? entry.folder.Value()
		                             : std::format(text::order::LOCATION_FORMAT, snapshot.modsDirectory, entry.folder.Value());

		design::ClippedText::Draw(ImVec2(rowTopLeft.x + 62.0f, leftY + 17.0f), leftWidth - 125.0f, location, tokens::SECONDARY_MUTED, 10.0f);

		if (index > 0) {
			if (design::Button(
				ImVec2(rowBottomRight.x - 54.0f, leftY + 6.0f),
				text::order::MOVE_UP,
				design::ButtonVariant::Neutral
			)) {
				orderService->Move(index, index - 1);
				apply = true;
				ImGui::PopID();
				break;
			}
		}

		if (index + 1 < snapshot.entries.size()) {
			if (design::Button(
				ImVec2(rowBottomRight.x - 28.0f, leftY + 6.0f),
				text::order::MOVE_DOWN,
				design::ButtonVariant::Neutral
			)) {
				orderService->Move(index, index + 1);
				apply = true;
				ImGui::PopID();
				break;
			}
		}

		if (blocking) {
			const domain::Annotation* const reason = AnnotationFor_(snapshot, entry.folder);

			if (reason != nullptr) {
				design::TextAt(
					ImVec2(rowTopLeft.x + 62.0f, leftY + 28.0f),
					reason->tag,
					tokens::FAILURE,
					10.0f
				);
				design::TextAt(
					ImVec2(rowTopLeft.x + 110.0f, leftY + 28.0f),
					reason->explanation,
					tokens::FAILURE,
					10.0f
				);
			}
		}

		design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

		leftY += rowHeight;
		++index;
		ImGui::PopID();
	}
	if (apply) {
		_bottom = leftY;
		ImGui::EndChild();
		ImGui::PopStyleVar();
		return true;
	}

	if (insertionY.has_value()) {
		ImGui::GetWindowDrawList()->AddLine(ImVec2(area.origin.x, *insertionY), ImVec2(rightX, *insertionY), tokens::ACCENT, 2.0f);
	}
	_bottom = leftY;

	ImGui::EndChild();
	ImGui::PopStyleVar();
	return _drag.Commit(*orderService);
}

float OrderEntryList::Bottom() const {
	return _bottom;
}

const std::string& OrderEntryList::Selected() const {
	return _selected;
}
}
