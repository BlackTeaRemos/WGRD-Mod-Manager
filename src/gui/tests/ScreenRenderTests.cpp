#include "gui/tests/ScreenRenderFixture.h"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <imgui_internal.h>

TEST_CASE("screens render across sizes") {
	using namespace wgrd::gui;
	tests::ScreenRenderFixture fixture;
	REQUIRE(fixture.Ready());
	constexpr std::array<float, 3> WIDTHS{500.0f, 800.0f, 1400.0f};
	constexpr std::array<Screen, 5> SCREENS{
		Screen::Catalog, Screen::Order, Screen::Transfers, Screen::Profiles, Screen::Publish
	};
	ApplicationState state;
	for (const float width : WIDTHS) {
		for (const Screen screen : SCREENS) {
			state.SetScreen(screen);
			for (int frame = 0; frame < 3; ++frame) {
				fixture.Draw(width, 420.0f, state);
			}
			CAPTURE(width, static_cast<int>(screen));
			const ImDrawData* drawing = ImGui::GetDrawData();
			REQUIRE(drawing != nullptr);
			REQUIRE(drawing->Valid);
			REQUIRE(drawing->TotalVtxCount > 0);
			REQUIRE(drawing->DisplaySize.x == width);
		}
	}
	state.OpenSettings();
	for (const float width : WIDTHS) {
		for (int frame = 0; frame < 3; ++frame) {
			fixture.Draw(width, 420.0f, state);
		}
		REQUIRE(state.SettingsOpen());
		REQUIRE(ImGui::GetDrawData()->TotalVtxCount > 0);
		const ImGuiWindow* dialog = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
		REQUIRE(dialog != nullptr);
		REQUIRE(dialog->Pos.x >= 0.0f);
		REQUIRE(dialog->Pos.y >= 0.0f);
		REQUIRE(dialog->Pos.x + dialog->Size.x <= width);
		REQUIRE(dialog->Pos.y + dialog->Size.y <= 420.0f);
	}
	ImGuiIO& input = ImGui::GetIO();
	input.AddKeyEvent(ImGuiMod_Ctrl, true);
	input.AddKeyEvent(ImGuiKey_1, true);
	fixture.Draw(500.0f, 420.0f, state);
	REQUIRE(state.ActiveScreen() == Screen::Publish);
	input.AddKeyEvent(ImGuiKey_1, false);
	input.AddKeyEvent(ImGuiMod_Ctrl, false);
	input.AddKeyEvent(ImGuiKey_Escape, true);
	fixture.Draw(500.0f, 420.0f, state);
	REQUIRE_FALSE(state.SettingsOpen());
	input.AddKeyEvent(ImGuiKey_Escape, false);
	fixture.Draw(500.0f, 420.0f, state);
	input.AddKeyEvent(ImGuiMod_Ctrl, true);
	input.AddKeyEvent(ImGuiKey_2, true);
	fixture.Draw(500.0f, 420.0f, state);
	REQUIRE(state.ActiveScreen() == Screen::Order);
}
