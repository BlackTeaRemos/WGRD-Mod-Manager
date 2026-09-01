#include "gui/design/FontLibrary.h"

#include <windows.h>

#include <cmath>
#include <filesystem>
#include <string>

namespace wgrd::gui::design {
namespace {
	constexpr std::wstring_view FACE_FILE = L"consola.ttf";

	std::array<ImFont*, FontLibrary::SIZES.size()> loaded{};

	std::filesystem::path FacePath() {
		std::array<wchar_t, MAX_PATH> directory{};

		const UINT written = GetWindowsDirectoryW(directory.data(), MAX_PATH);
		if (written == 0 || written >= MAX_PATH) {
			return {};
		}

		return std::filesystem::path(directory.data()) / L"Fonts" / FACE_FILE;
	}
}

std::size_t FontLibrary::NearestIndex_(const float size) {
	std::size_t nearest = 0;
	float best = std::fabs(SIZES[0] - size);

	for (std::size_t index = 1; index < SIZES.size(); ++index) {
		const float distance = std::fabs(SIZES[index] - size);

		if (distance < best) {
			best = distance;
			nearest = index;
		}
	}

	return nearest;
}

void FontLibrary::Build() {
	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->Clear();
	loaded.fill(nullptr);

	const std::filesystem::path face = FacePath();

	std::error_code probe;
	const bool present = !face.empty() && std::filesystem::is_regular_file(face, probe) && !probe;

	const std::string facePath = present ? face.string() : std::string();

	for (std::size_t index = 0; index < SIZES.size(); ++index) {
		const float pixels = SIZES[index];

		if (!facePath.empty()) {
			loaded[index] = io.Fonts->AddFontFromFileTTF(facePath.c_str(), pixels);
		}

		if (loaded[index] != nullptr) {
			continue;
		}

		ImFontConfig fallback;
		fallback.SizePixels = pixels;

		loaded[index] = io.Fonts->AddFontDefault(&fallback);
	}
}

ImFont* FontLibrary::For(const float size) {
	ImFont* const chosen = loaded[NearestIndex_(size)];

	return chosen != nullptr ? chosen : ImGui::GetFont();
}
}
