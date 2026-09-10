#include "gui/modals/Modals.h"

#include "gui/modals/ModalSurface.h"

#include <imgui.h>

namespace wgrd::gui {
void Modals::Draw(ApplicationState& state, const ApplicationServices& services) {
	if (state.SettingsOpen()) {
		if (!ImGui::IsPopupOpen("Settings")) {
			ImGui::OpenPopup("Settings");
		}
		bool open = true;
		if (ModalSurface::Begin("Settings", &open, 520.0f)) {
			_settings.Draw(state, services);
			ModalSurface::End();
		}
		if (!open) {
			state.CloseSettings();
		}
	}
	if (state.DetailOpen()) {
		if (!ImGui::IsPopupOpen("Mod details")) {
			ImGui::OpenPopup("Mod details");
		}
		bool open = true;
		if (ModalSurface::Begin("Mod details", &open, 560.0f)) {
			_details.Draw(state, services);
			ModalSurface::End();
		}
		if (!open) {
			state.CloseDetail();
		}
	}
}
}
