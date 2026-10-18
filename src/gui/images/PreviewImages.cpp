#include "gui/images/PreviewImages.h"
#include "gui/images/PngTextureDecoder.h"

#include <algorithm>
#include <Windows.h>
#include <objbase.h>

namespace wgrd::gui {
void PreviewImages::Initialize() {
	_comInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
}

bool PreviewImages::Draw(const std::string& identity, const std::span<const std::uint8_t> bytes,
	const ImVec2 origin, const ImVec2 bounds, const bool thumbnail) {
	if (bytes.empty() || bounds.x <= 0 || bounds.y <= 0) {
		return false;
	}
	const std::string key = identity + (thumbnail ? "/thumbnail" : "/preview");
	auto found = _textures.find(key);
	if (found == _textures.end()) {
		if (_textures.size() >= CACHE_LIMIT) {
			return false;
		}
		auto texture = std::make_unique<ImTextureData>();
		const bool decoded = PngTextureDecoder::Decode(bytes, thumbnail ? 64 : 1024, *texture);
		if (!decoded) {
			texture->DestroyPixels();
		}
		found = _textures.emplace(key, std::make_pair(ImGui::GetFrameCount(), std::move(texture))).first;
	}
	found->second.first = ImGui::GetFrameCount();
	ImTextureData& texture = *found->second.second;
	if (texture.Pixels == nullptr || texture.WantDestroyNextFrame) {
		return false;
	}
	const float scale = std::min(bounds.x / static_cast<float>(texture.Width), bounds.y / static_cast<float>(texture.Height));
	const ImVec2 size(static_cast<float>(texture.Width) * scale, static_cast<float>(texture.Height) * scale);
	const ImVec2 start(origin.x + (bounds.x - size.x) * 0.5f, origin.y + (bounds.y - size.y) * 0.5f);
	ImGui::GetWindowDrawList()->AddImage(texture.GetTexRef(), start, ImVec2(start.x + size.x, start.y + size.y));
	return true;
}

void PreviewImages::RegisterTextures() {
	for (auto entry = _textures.begin(); entry != _textures.end();) {
		ImTextureData& texture = *entry->second.second;
		if (texture.WantDestroyNextFrame && texture.Status == ImTextureStatus_Destroyed) {
			entry = _textures.erase(entry);
			continue;
		}
		if (ImGui::GetFrameCount() - entry->second.first > 2) {
			if (texture.Pixels == nullptr) {
				entry = _textures.erase(entry);
				continue;
			}
			texture.WantDestroyNextFrame = true;
			texture.UnusedFrames = ImGui::GetFrameCount() - entry->second.first;
			texture.SetStatus(ImTextureStatus_WantDestroy);
		}
		if (texture.Pixels != nullptr) {
			ImGui::GetPlatformIO().Textures.push_back(&texture);
		}
		++entry;
	}
}

void PreviewImages::Clear() {
	ImGui::GetPlatformIO().Textures.resize(0);
	_textures.clear();
	if (_comInitialized) {
		CoUninitialize();
		_comInitialized = false;
	}
}
}
