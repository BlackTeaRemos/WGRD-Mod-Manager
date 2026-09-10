#include "gui/design/UiStyle.h"

#include "gui/design/Tokens.h"

namespace wgrd::gui::design {
void UiStyle::SetColor_(ImGuiStyle& style, const ImGuiCol slot, const ImU32 color) {
	style.Colors[slot] = ImGui::ColorConvertU32ToFloat4(color);
}

void UiStyle::Apply() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.ChildRounding = 0.0f;
	style.PopupRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 0.0f;
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.WindowPadding = ImVec2(6.0f, 6.0f);
	style.FramePadding = ImVec2(6.0f, 3.0f);
	style.ItemSpacing = ImVec2(6.0f, 4.0f);
	style.ScrollbarSize = 12.0f;
	for (ImVec4& color : style.Colors) {
		color = ImGui::ColorConvertU32ToFloat4(tokens::PRIMARY);
	}
	SetColor_(style, ImGuiCol_Text, tokens::SECONDARY);
	SetColor_(style, ImGuiCol_TextDisabled, tokens::TEXT_DISABLED);
	SetColor_(style, ImGuiCol_WindowBg, tokens::PRIMARY);
	SetColor_(style, ImGuiCol_ChildBg, tokens::PRIMARY);
	SetColor_(style, ImGuiCol_PopupBg, tokens::SURFACE_RAISED);
	SetColor_(style, ImGuiCol_Border, tokens::BORDER);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	SetColor_(style, ImGuiCol_FrameBg, tokens::SURFACE_SUNKEN);
	SetColor_(style, ImGuiCol_FrameBgHovered, tokens::ACCENT_HOVER_FILL);
	SetColor_(style, ImGuiCol_FrameBgActive, tokens::ACCENT_ACTIVE_FILL);
	SetColor_(style, ImGuiCol_TitleBg, tokens::ACCENT);
	SetColor_(style, ImGuiCol_TitleBgActive, tokens::ACCENT);
	SetColor_(style, ImGuiCol_TitleBgCollapsed, tokens::SURFACE_RAISED);
	SetColor_(style, ImGuiCol_MenuBarBg, tokens::SURFACE_RAISED);
	SetColor_(style, ImGuiCol_ScrollbarGrab, tokens::BORDER);
	SetColor_(style, ImGuiCol_ScrollbarGrabHovered, tokens::SECONDARY_MUTED);
	SetColor_(style, ImGuiCol_ScrollbarGrabActive, tokens::ACCENT);
	SetColor_(style, ImGuiCol_CheckMark, tokens::SUCCESS);
	SetColor_(style, ImGuiCol_SliderGrab, tokens::ACCENT);
	SetColor_(style, ImGuiCol_SliderGrabActive, tokens::ACCENT_HOVER);
	SetColor_(style, ImGuiCol_Button, tokens::ACCENT_BUTTON_FILL);
	SetColor_(style, ImGuiCol_ButtonHovered, tokens::ACCENT_BUTTON_HOVER);
	SetColor_(style, ImGuiCol_ButtonActive, tokens::ACCENT_ACTIVE_FILL);
	SetColor_(style, ImGuiCol_Header, tokens::ACCENT_ACTIVE_FILL);
	SetColor_(style, ImGuiCol_HeaderHovered, tokens::ACCENT_HOVER_FILL);
	SetColor_(style, ImGuiCol_HeaderActive, tokens::ACCENT_ACTIVE_FILL);
	SetColor_(style, ImGuiCol_Separator, tokens::BORDER);
	SetColor_(style, ImGuiCol_SeparatorHovered, tokens::ACCENT);
	SetColor_(style, ImGuiCol_SeparatorActive, tokens::ACCENT_HOVER);
	SetColor_(style, ImGuiCol_ResizeGrip, tokens::ACCENT_BUTTON_FILL);
	SetColor_(style, ImGuiCol_ResizeGripHovered, tokens::ACCENT);
	SetColor_(style, ImGuiCol_ResizeGripActive, tokens::ACCENT_HOVER);
	SetColor_(style, ImGuiCol_PlotHistogram, tokens::ACCENT);
	SetColor_(style, ImGuiCol_TextSelectedBg, tokens::ACCENT_ACTIVE_FILL);
	SetColor_(style, ImGuiCol_NavCursor, tokens::ACCENT);
	SetColor_(style, ImGuiCol_DragDropTarget, tokens::ACCENT);
	SetColor_(style, ImGuiCol_ModalWindowDimBg, tokens::SHADOW);
}
}
