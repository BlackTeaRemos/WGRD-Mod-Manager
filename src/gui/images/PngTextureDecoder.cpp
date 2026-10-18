#include "gui/images/PngTextureDecoder.h"
#include "domain/types/distribution/TransportLimits.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>

namespace wgrd::gui {
bool PngTextureDecoder::Decode(const std::span<const std::uint8_t> bytes, const int maximumSize, ImTextureData& texture) {
	if (bytes.empty() || bytes.size() > domain::limits::PREVIEW_IMAGE_BYTES) {
		return false;
	}
	Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
		return false;
	}
	Microsoft::WRL::ComPtr<IWICStream> stream;
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(
		const_cast<BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()))) ||
		FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) {
		return false;
	}
	GUID format{};
	if (FAILED(decoder->GetContainerFormat(&format)) || format != GUID_ContainerFormatPng ||
		FAILED(decoder->GetFrame(0, &frame))) {
		return false;
	}
	UINT width = 0;
	UINT height = 0;
	if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0 || width > 2048 || height > 2048) {
		return false;
	}
	const float scale = std::min(1.0f, static_cast<float>(maximumSize) / static_cast<float>(std::max(width, height)));
	const UINT outputWidth = std::max(1u, static_cast<UINT>(static_cast<float>(width) * scale));
	const UINT outputHeight = std::max(1u, static_cast<UINT>(static_cast<float>(height) * scale));
	Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
	Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
	if (FAILED(factory->CreateBitmapScaler(&scaler)) ||
		FAILED(scaler->Initialize(frame.Get(), outputWidth, outputHeight, WICBitmapInterpolationModeFant)) ||
		FAILED(factory->CreateFormatConverter(&converter)) ||
		FAILED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
			nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
		return false;
	}
	texture.Create(ImTextureFormat_RGBA32, static_cast<int>(outputWidth), static_cast<int>(outputHeight));
	if (FAILED(converter->CopyPixels(nullptr, outputWidth * 4, outputWidth * outputHeight * 4, texture.Pixels))) {
		texture.DestroyPixels();
		return false;
	}
	texture.UseColors = true;
	texture.RefCount = 1;
	return true;
}
}
