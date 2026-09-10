#include "gui/design/FontLibrary.h"

#include "gui/fonts/CompressedFont.h"
#include "fonts/InterRegular.h"
#include "fonts/InterSemiBold.h"
#include "fonts/InterBold.h"

#include <imgui_freetype.h>
#include <algorithm>

namespace wgrd::gui::design {
bool FontLibrary::Build() {
	ImFontAtlas& atlas = *ImGui::GetIO().Fonts;
	atlas.Clear();
	atlas.SetFontLoader(ImGuiFreeType::GetFontLoader());
	_fonts = {
		CompressedFont::Add(fonts::INTER_REGULAR, "Inter Regular"),
		CompressedFont::Add(fonts::INTER_SEMIBOLD, "Inter Semibold"),
		CompressedFont::Add(fonts::INTER_BOLD, "Inter Bold"),
		CompressedFont::Add(fonts::INTER_REGULAR, "Inter Regular"),
		CompressedFont::Add(fonts::INTER_SEMIBOLD, "Inter Semibold")
	};
	if (std::ranges::any_of(_fonts, [](const ImFont* font) {
		return font == nullptr;
	})) {
		return false;
	}
	ImGui::GetIO().FontDefault = For(FontRole::Body);
	return true;
}

ImFont* FontLibrary::For(const FontRole role) {
	return _fonts[static_cast<std::size_t>(role)];
}
}
