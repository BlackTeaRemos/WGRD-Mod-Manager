#include "gui/design/inputs/TextEntry.h"

#include "gui/design/FontLibrary.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"

#include <string>

namespace wgrd::gui::design {
bool TextEntry::Draw(const ImVec2 topLeft, const float width, const std::string_view identity,
	char* buffer, const std::size_t capacity, const ImGuiInputTextFlags flags) {
	const ImVec2 bottomRight(topLeft.x + width, topLeft.y + FIELD_HEIGHT);
	FillRect(topLeft, bottomRight, tokens::SURFACE_SUNKEN);
	StrokeRect(topLeft, bottomRight, tokens::BORDER);
	ImGui::SetCursorScreenPos(ImVec2(topLeft.x + 6.0f, topLeft.y + 4.0f));
	ImGui::PushItemWidth(width - 12.0f);
	ImGui::PushFont(FontLibrary::For(FontRole::Data), 11.0f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, tokens::SECONDARY);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
	const std::string label = "##" + std::string(identity);
	const bool changed = ImGui::InputText(label.c_str(), buffer, capacity, flags);
	if (ImGui::IsItemFocused()) {
		StrokeRect(topLeft, bottomRight, tokens::ACCENT);
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
	ImGui::PopFont();
	ImGui::PopItemWidth();
	return changed;
}
}
