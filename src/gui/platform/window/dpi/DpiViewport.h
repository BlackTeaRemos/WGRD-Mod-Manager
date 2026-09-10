#pragma once

#include <imgui.h>

namespace wgrd::gui {
class DpiViewport {
public:
	void Initialize();
	void Prepare(float scale);
	void Shutdown();
	static void BeginNativeInput();
	static void EndNativeInput();

private:
	static void SetIme_(ImGuiContext* context, ImGuiViewport* viewport, ImGuiPlatformImeData* data);
	static void ScalePosition_(ImVec2& position, float ratio);
	static void ScaleInput_(ImGuiContext& context, float ratio);

	void (*_nativeIme)(ImGuiContext*, ImGuiViewport*, ImGuiPlatformImeData*) = nullptr;
	void* _previousUserData = nullptr;
	int _nativeDepth = 0;
	float _scale = 1.0f;
};
}
