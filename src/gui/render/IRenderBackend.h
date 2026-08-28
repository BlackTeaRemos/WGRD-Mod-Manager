#pragma once

#include <Windows.h>

#include <cstdint>
#include <string_view>

struct ImDrawData;

namespace wgrd::gui {
enum class RenderBackendKind {
	Direct3D11
	, Direct3D12
	, Vulkan
	, OpenGL3
};

class IRenderBackend {
public:
	virtual ~IRenderBackend() = 0;

	[[nodiscard]] virtual bool Initialize(HWND window) = 0;
	virtual void Shutdown() = 0;

	virtual void BeginFrame() = 0;
	virtual void Present(ImDrawData* drawData) = 0;
	virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;

	[[nodiscard]] virtual RenderBackendKind Kind() const = 0;
	[[nodiscard]] virtual std::string_view Name() const = 0;
};

inline IRenderBackend::~IRenderBackend() = default;
}
