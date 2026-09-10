#include "gui/platform/window/dpi/DpiViewport.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>

namespace wgrd::gui {
void DpiViewport::Initialize() {
	ImGuiIO& input = ImGui::GetIO();
	_previousUserData = input.UserData;
	input.UserData = this;
	input.ConfigNavMoveSetMousePos = false;
	_nativeIme = ImGui::GetPlatformIO().Platform_SetImeDataFn;
	ImGui::GetPlatformIO().Platform_SetImeDataFn = SetIme_;
}

void DpiViewport::Shutdown() {
	ImGui::GetPlatformIO().Platform_SetImeDataFn = _nativeIme;
	ImGui::GetIO().UserData = _previousUserData;
}

void DpiViewport::ScalePosition_(ImVec2& position, const float ratio) {
	if (position.x > -FLT_MAX / 2.0f) {
		position.x *= ratio;
	}
	if (position.y > -FLT_MAX / 2.0f) {
		position.y *= ratio;
	}
}

void DpiViewport::ScaleInput_(ImGuiContext& context, const float ratio) {
	ScalePosition_(context.IO.MousePos, ratio);
	for (ImGuiInputEvent& event : context.InputEventsQueue) {
		if (event.Type == ImGuiInputEventType_MousePos) {
			ImVec2 position(event.MousePos.PosX, event.MousePos.PosY);
			ScalePosition_(position, ratio);
			event.MousePos.PosX = position.x;
			event.MousePos.PosY = position.y;
		}
	}
}

void DpiViewport::BeginNativeInput() {
	ImGuiContext* context = ImGui::GetCurrentContext();
	if (context == nullptr || context->IO.UserData == nullptr) {
		return;
	}
	auto* self = static_cast<DpiViewport*>(context->IO.UserData);
	if (self->_nativeDepth++ == 0) {
		ScaleInput_(*context, self->_scale);
	}
}

void DpiViewport::EndNativeInput() {
	ImGuiContext* context = ImGui::GetCurrentContext();
	if (context == nullptr || context->IO.UserData == nullptr) {
		return;
	}
	auto* self = static_cast<DpiViewport*>(context->IO.UserData);
	if (self->_nativeDepth > 0 && --self->_nativeDepth == 0) {
		ScaleInput_(*context, 1.0f / self->_scale);
	}
}

void DpiViewport::Prepare(float scale) {
	scale = std::clamp(scale, 0.5f, 8.0f);
	ImGuiContext& context = *ImGui::GetCurrentContext();
	ImGuiIO& input = context.IO;
	const float ratio = _scale / scale;
	ScaleInput_(context, ratio);
	ScalePosition_(input.MousePosPrev, ratio);
	for (ImVec2& position : input.MouseClickedPos) {
		ScalePosition_(position, ratio);
	}
	_scale = scale;
	input.DisplaySize.x /= scale;
	input.DisplaySize.y /= scale;
	input.DisplayFramebufferScale = ImVec2(scale, scale);
}

void DpiViewport::SetIme_(ImGuiContext* context, ImGuiViewport* viewport, ImGuiPlatformImeData* data) {
	auto* self = static_cast<DpiViewport*>(context->IO.UserData);
	if (self == nullptr || self->_nativeIme == nullptr) {
		return;
	}
	ImGuiPlatformImeData physical = *data;
	physical.InputPos.x *= self->_scale;
	physical.InputPos.y *= self->_scale;
	physical.InputLineHeight *= self->_scale;
	self->_nativeIme(context, viewport, &physical);
}
}
