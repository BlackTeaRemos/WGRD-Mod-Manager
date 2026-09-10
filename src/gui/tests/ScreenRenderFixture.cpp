#include "gui/tests/ScreenRenderFixture.h"

#include "gui/design/FontLibrary.h"
#include "gui/design/UiStyle.h"
#include "gui/state/KeyboardNavigation.h"

namespace wgrd::gui::tests {
ScreenRenderFixture::ScreenRenderFixture()
	: _context(ImGui::CreateContext())
	, _ready(design::FontLibrary::Build()) {
	ImGuiIO& input = ImGui::GetIO();
	input.IniFilename = nullptr;
	input.LogFilename = nullptr;
	input.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
	input.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	design::UiStyle::Apply();
}

ScreenRenderFixture::~ScreenRenderFixture() {
	ImGui::DestroyContext(_context);
}

bool ScreenRenderFixture::Ready() const {
	return _ready;
}

void ScreenRenderFixture::Draw(const float width, const float height, ApplicationState& state) {
	ImGuiIO& input = ImGui::GetIO();
	input.DisplaySize = ImVec2(width, height);
	input.DeltaTime = 1.0f / 60.0f;
	ImGui::NewFrame();
	KeyboardNavigation::Apply(state);
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(input.DisplaySize);
	ImGui::Begin("frame", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
	const ApplicationServices services;
	const ImVec2 available = ImGui::GetContentRegionAvail();
	_screens.Draw(ScreenArea{ImGui::GetCursorScreenPos(), available.x, available.y}, state, services);
	_modals.Draw(state, services);
	ImGui::End();
	ImGui::Render();
}
}
