#include "manager/preview/PreviewImageReader.h"
#include "manager/preview/PreviewImageCodec.h"
#include "domain/types/distribution/TransportLimits.h"

#include <fstream>

namespace wgrd::manager {
std::optional<std::vector<std::uint8_t>> PreviewImageReader::ReadFile_(const std::filesystem::path& path, const std::size_t limit) {
	std::error_code failure;
	const bool exists = std::filesystem::exists(path, failure);
	if (failure) {
		return std::nullopt;
	}
	if (!exists) {
		return std::vector<std::uint8_t>{};
	}
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	const auto size = input.tellg();
	if (!input || size <= 0 || size > static_cast<std::streamoff>(limit)) {
		return std::nullopt;
	}
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	input.seekg(0);
	if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())) || !PreviewImageCodec::IsPng(bytes)) {
		return std::nullopt;
	}
	return bytes;
}

std::optional<domain::ModPreview> PreviewImageReader::Read(const std::filesystem::path& folder) {
	auto thumbnail = ReadFile_(folder / "thumbnail.png", domain::limits::THUMBNAIL_BYTES);
	auto image = ReadFile_(folder / "preview.png", domain::limits::PREVIEW_IMAGE_BYTES);
	if (!thumbnail || !image) {
		return std::nullopt;
	}
	return domain::ModPreview{std::move(*thumbnail), std::move(*image)};
}
}
