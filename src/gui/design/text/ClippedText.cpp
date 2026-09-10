#include "gui/design/text/ClippedText.h"

#include "gui/design/FontLibrary.h"
#include "gui/design/layout/CanvasExtent.h"

#include <algorithm>

namespace wgrd::gui::design {
void ClippedText::Draw(const ImVec2 origin, const float width, const std::string_view value,
	const ImU32 color, const float size, const FontRole role) {
	if (width <= 0.0f) {
		return;
	}
	const ImVec2 end(origin.x + width, origin.y + size + 3.0f);
	ImDrawList* canvas = ImGui::GetWindowDrawList();
	ImGui::PushFont(FontLibrary::For(role), size);
	const ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, value.data(), value.data() + value.size());
	canvas->PushClipRect(origin, end, true);
	canvas->AddText(ImGui::GetFont(), size, origin, color, value.data(), value.data() + value.size());
	canvas->PopClipRect();
	CanvasExtent::Include(ImVec2(origin.x + std::min(extent.x, width), end.y));
	if (extent.x > width && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(origin, end)) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 48.0f);
		ImGui::TextUnformatted(value.data(), value.data() + value.size());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
	ImGui::PopFont();
}
}
