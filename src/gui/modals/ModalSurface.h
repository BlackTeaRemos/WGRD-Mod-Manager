#pragma once

namespace wgrd::gui {
class ModalSurface {
public:
	[[nodiscard]] static bool Begin(const char* title, bool* open, float width);
	[[nodiscard]] static bool BeginMenu(const char* title, float width);
	static void End();

private:
	static void Clamp_();
	static void Shadow_();
};
}
