#include "manager/preview/PreviewImageCodec.h"

#include <sodium.h>
#include <algorithm>
#include <array>

namespace wgrd::manager {
bool PreviewImageCodec::IsPng(const std::span<const std::uint8_t> bytes) {
	constexpr std::array<std::uint8_t, 8> SIGNATURE{137, 80, 78, 71, 13, 10, 26, 10};
	if (bytes.size() < 33 || !std::equal(SIGNATURE.begin(), SIGNATURE.end(), bytes.begin())) {
		return false;
	}
	for (const std::size_t offset : {16u, 20u}) {
		const std::uint32_t dimension = (static_cast<std::uint32_t>(bytes[offset]) << 24)
			| (static_cast<std::uint32_t>(bytes[offset + 1]) << 16)
			| (static_cast<std::uint32_t>(bytes[offset + 2]) << 8)
			| static_cast<std::uint32_t>(bytes[offset + 3]);
		if (dimension == 0 || dimension > 2048) {
			return false;
		}
	}
	return true;
}

std::string PreviewImageCodec::Encode(const std::span<const std::uint8_t> bytes) {
	std::string encoded(sodium_base64_ENCODED_LEN(bytes.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
	sodium_bin2base64(encoded.data(), encoded.size(), bytes.data(), bytes.size(), sodium_base64_VARIANT_ORIGINAL);
	encoded.pop_back();
	return encoded;
}

std::optional<std::vector<std::uint8_t>> PreviewImageCodec::Decode(const std::string_view text, const std::size_t limit) {
	if (text.size() > sodium_base64_ENCODED_LEN(limit, sodium_base64_VARIANT_ORIGINAL)) {
		return std::nullopt;
	}
	std::vector<std::uint8_t> decoded(limit);
	std::size_t written = 0;
	if (sodium_base642bin(decoded.data(), decoded.size(), text.data(), text.size(), nullptr,
		&written, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0) {
		return std::nullopt;
	}
	decoded.resize(written);
	if (!decoded.empty() && !IsPng(decoded)) {
		return std::nullopt;
	}
	return decoded;
}
}
