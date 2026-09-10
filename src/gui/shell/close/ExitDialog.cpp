#include "gui/shell/close/ExitDialog.h"
#include "gui/modals/ModalSurface.h"

#include <imgui.h>

namespace wgrd::gui {
void ExitDialog::Open() {
	_requested = true;
}

WindowCloseChoice ExitDialog::Draw(ApplicationState& state) {
	if (_requested) {
		ImGui::OpenPopup("Active operation");
		_requested = false;
	}
	WindowCloseChoice choice = WindowCloseChoice::None;
	if (ModalSurface::Begin("Active operation", nullptr, 380.0f)) {
		ImGui::TextUnformatted("Work is still active");
		bool tray = state.BackgroundOnClose();
		if (ImGui::Checkbox("Minimize to tray", &tray)) {
			state.SetBackgroundOnClose(tray);
		}
		if (ImGui::Button("Keep open") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			choice = WindowCloseChoice::KeepOpen;
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Keep working")) {
			choice = WindowCloseChoice::Background;
		}
		ImGui::SameLine();
		if (ImGui::Button("Exit")) {
			choice = WindowCloseChoice::Exit;
		}
		if (choice != WindowCloseChoice::None) {
			ImGui::CloseCurrentPopup();
		}
		ModalSurface::End();
	}
	return choice;
}
}
