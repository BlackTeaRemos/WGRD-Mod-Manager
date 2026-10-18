#pragma once

#include <imgui.h>
#include <cstdint>
#include <span>
#include <string>
#include <map>
#include <memory>

namespace wgrd::gui {
class PreviewImages {
public:
	static void Initialize();
	[[nodiscard]] static bool Draw(const std::string& identity, std::span<const std::uint8_t> bytes, ImVec2 origin, ImVec2 bounds, bool thumbnail);
	static void RegisterTextures();
	static void Clear();
private:
	static constexpr std::size_t CACHE_LIMIT = 64;
	inline static bool _comInitialized = false;
	inline static std::map<std::string, std::pair<int, std::unique_ptr<ImTextureData>>> _textures;
};
}
