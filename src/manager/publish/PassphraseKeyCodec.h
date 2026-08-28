#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
enum class PassphraseKeyCodecError {
	PassphraseTooShort
	, PublisherRejected
	, SecretKeyRejected
	, HeaderRejected
	, SizeRejected
	, DerivationFailed
	, PassphraseRejected
};

class PassphraseKeyCodec {
public:
	struct OpenedKey {
		std::vector<std::uint8_t> secretKey;
		std::string publisherName;
	};

	static constexpr std::size_t MINIMUM_PASSPHRASE_LENGTH = 8;
	static constexpr std::size_t MAXIMUM_SEALED_BYTES = 4096;

	[[nodiscard]] static std::expected<std::vector<std::uint8_t>, PassphraseKeyCodecError> Seal(
		std::span<const std::uint8_t> secretKey,
		std::string_view publisherName,
		std::string_view passphrase
	);

	[[nodiscard]] static std::expected<OpenedKey, PassphraseKeyCodecError> Open(
		std::span<const std::uint8_t> sealed,
		std::string_view passphrase
	);
};
}
