#include "gui/fonts/CompressedFont.h"

#include <imgui_freetype.h>
#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <limits>

namespace wgrd::gui {
ImFont* CompressedFont::Add(const std::span<const unsigned char> compressed, const char* name) {
	if (compressed.size() < 18 || compressed.size() > std::numeric_limits<uInt>::max()) {
		return nullptr;
	}
	const std::size_t trailer = compressed.size() - 4;
	const std::uint32_t size = static_cast<std::uint32_t>(compressed[trailer])
		| (static_cast<std::uint32_t>(compressed[trailer + 1]) << 8)
		| (static_cast<std::uint32_t>(compressed[trailer + 2]) << 16)
		| (static_cast<std::uint32_t>(compressed[trailer + 3]) << 24);
	if (size == 0 || size > 16u * 1024u * 1024u) {
		return nullptr;
	}
	auto* bytes = static_cast<unsigned char*>(ImGui::MemAlloc(size));
	if (bytes == nullptr) {
		return nullptr;
	}
	z_stream stream{};
	stream.next_in = const_cast<unsigned char*>(compressed.data());
	stream.avail_in = static_cast<uInt>(compressed.size());
	stream.next_out = bytes;
	stream.avail_out = size;
	if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
		ImGui::MemFree(bytes);
		return nullptr;
	}
	const int result = inflate(&stream, Z_FINISH);
	const bool complete = result == Z_STREAM_END && stream.total_out == size && stream.avail_in == 0;
	inflateEnd(&stream);
	if (!complete) {
		ImGui::MemFree(bytes);
		return nullptr;
	}
	ImFontConfig configuration;
	configuration.FontDataOwnedByAtlas = true;
	configuration.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
	configuration.SizePixels = 13.0f;
	strncpy_s(configuration.Name, name, _TRUNCATE);
	return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(bytes, static_cast<int>(size), 13.0f, &configuration);
}
}
