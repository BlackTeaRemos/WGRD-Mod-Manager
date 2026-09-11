#include "gui/design/FontLibrary.h"

namespace wgrd::gui::design {
bool FontLibrary::Build() {
	ImFontAtlas& atlas = *ImGui::GetIO().Fonts;
	atlas.Clear();
	ImFont* const defaultFont = atlas.AddFontDefault();
	if (defaultFont == nullptr) {
		return false;
	}
	_fonts.fill(defaultFont);
	ImGui::GetIO().FontDefault = For(FontRole::Body);
	return true;
}

ImFont* FontLibrary::For(const FontRole role) {
	return _fonts[static_cast<std::size_t>(role)];
}
}
