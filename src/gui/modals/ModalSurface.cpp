#include "gui/modals/ModalSurface.h"

#include "gui/design/FontLibrary.h"
#include "gui/design/Tokens.h"

#include <algorithm>

namespace wgrd::gui {
bool ModalSurface::Begin(const char* title, bool* open, const float width) {
	const ImGuiViewport& viewport = *ImGui::GetMainViewport();
	const float maximumWidth = std::max(200.0f, viewport.WorkSize.x - 16.0f);
	const float maximumHeight = std::max(100.0f, viewport.WorkSize.y - 16.0f);
	ImGui::SetNextWindowPos(viewport.GetWorkCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(std::min(width, maximumWidth), 0.0f), ImGuiCond_Appearing);
	ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(280.0f, maximumWidth), 100.0f),
		ImVec2(maximumWidth, maximumHeight));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushFont(design::FontLibrary::For(FontRole::Heading), 13.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, tokens::PRIMARY);
	const bool visible = ImGui::BeginPopupModal(title, open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::PopStyleColor();
	ImGui::PopFont();
	if (!visible) {
		ImGui::PopStyleVar(2);
		return false;
	}
	ImGui::PushFont(design::FontLibrary::For(FontRole::Label), 11.0f);
	Clamp_();
	Shadow_();
	return true;
}

bool ModalSurface::BeginMenu(const char* title, const float width) {
	const ImGuiViewport& viewport = *ImGui::GetMainViewport();
	ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(width, viewport.WorkSize.x - 16.0f), 0.0f),
		ImVec2(viewport.WorkSize.x - 16.0f, viewport.WorkSize.y - 16.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	if (!ImGui::BeginPopup(title)) {
		ImGui::PopStyleVar(2);
		return false;
	}
	ImGui::PushFont(design::FontLibrary::For(FontRole::Label), 11.0f);
	Clamp_();
	Shadow_();
	return true;
}

void ModalSurface::End() {
	ImGui::PopFont();
	ImGui::EndPopup();
	ImGui::PopStyleVar(2);
}

void ModalSurface::Clamp_() {
	const ImGuiViewport& viewport = *ImGui::GetMainViewport();
	const ImVec2 size = ImGui::GetWindowSize();
	const ImVec2 position = ImGui::GetWindowPos();
	const float right = std::max(viewport.WorkPos.x, viewport.WorkPos.x + viewport.WorkSize.x - size.x);
	const float bottom = std::max(viewport.WorkPos.y, viewport.WorkPos.y + viewport.WorkSize.y - size.y);
	ImGui::SetWindowPos(ImVec2(std::clamp(position.x, viewport.WorkPos.x, right),
		std::clamp(position.y, viewport.WorkPos.y, bottom)));
}

void ModalSurface::Shadow_() {
	const ImVec2 origin = ImGui::GetWindowPos();
	const ImVec2 size = ImGui::GetWindowSize();
	const float right = origin.x + size.x;
	const float bottom = origin.y + size.y;
	ImDrawList* canvas = ImGui::GetWindowDrawList();
	canvas->PushClipRectFullScreen();
	canvas->AddRectFilled(ImVec2(right, origin.y + tokens::SHADOW_OFFSET),
		ImVec2(right + tokens::SHADOW_OFFSET, bottom + tokens::SHADOW_OFFSET), tokens::SHADOW);
	canvas->AddRectFilled(ImVec2(origin.x + tokens::SHADOW_OFFSET, bottom),
		ImVec2(right, bottom + tokens::SHADOW_OFFSET), tokens::SHADOW);
	canvas->PopClipRect();
}
}
