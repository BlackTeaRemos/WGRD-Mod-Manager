#include "gui/tests/DpiViewportFixture.h"

#include <imgui_internal.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("DPI input preserves ordering") {
	wgrd::gui::tests::DpiViewportFixture fixture;
	ImGuiIO& input = ImGui::GetIO();
	input.DisplaySize = ImVec2(800.0f, 600.0f);
	fixture.Viewport().Prepare(2.0f);
	input.MousePos = ImVec2(100.0f, 100.0f);

	SECTION("physical position remains distinct") {
		wgrd::gui::DpiViewport::BeginNativeInput();
		input.AddMousePosEvent(100.0f, 100.0f);
		input.AddMouseButtonEvent(0, true);
		wgrd::gui::DpiViewport::EndNativeInput();
		const auto& events = ImGui::GetCurrentContext()->InputEventsQueue;
		REQUIRE(events.Size == 2);
		REQUIRE(events[0].Type == ImGuiInputEventType_MousePos);
		REQUIRE(events[0].MousePos.PosX == 50.0f);
		REQUIRE(events[1].Type == ImGuiInputEventType_MouseButton);
		REQUIRE(input.MousePos.x == 100.0f);
	}

	SECTION("monitor change rescales queued") {
		input.AddMousePosEvent(40.0f, 60.0f);
		input.DisplaySize = ImVec2(800.0f, 600.0f);
		fixture.Viewport().Prepare(1.0f);
		const auto& event = ImGui::GetCurrentContext()->InputEventsQueue.back();
		REQUIRE(event.MousePos.PosX == 80.0f);
		REQUIRE(input.MousePos.x == 200.0f);
		REQUIRE(input.DisplaySize.x == 800.0f);
	}
}
