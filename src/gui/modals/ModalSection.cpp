#include "gui/modals/ModalSection.h"

#include "gui/design/Primitives.h"

namespace wgrd::gui {
void ModalSection::Draw(const std::string_view title) {
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float width = ImGui::GetContentRegionAvail().x;
	design::HeadingBar(origin, width, title, design::HeadingLevel::Minor);
	ImGui::Dummy(ImVec2(width, design::HeadingHeight(design::HeadingLevel::Minor) + 6.0f));
}
}
