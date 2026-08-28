#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace wgrd::domain {
enum class FixedBytesError {
	WrongLength, IllegalCharacter
};

template <std::size_t LENGTH>
class FixedBytes {
public:
	static constexpr std::size_t BYTE_COUNT = LENGTH;
	static constexpr std::size_t HEX_LENGTH = LENGTH * 2;

	constexpr FixedBytes() noexcept
		: _bytes{} {}

	static std::expected<FixedBytes, FixedBytesError> FromBytes(std::span<const std::uint8_t> source) {
		if (source.size() != LENGTH) {
			return std::unexpected(FixedBytesError::WrongLength);
		}

		FixedBytes result;
		for (std::size_t position = 0; position < LENGTH; ++position) {
			result._bytes[position] = source[position];
		}
		return result;
	}

	static std::expected<FixedBytes, FixedBytesError> FromHex(const std::string_view text) {
		if (text.size() != HEX_LENGTH) {
			return std::unexpected(FixedBytesError::WrongLength);
		}

		FixedBytes result;
		for (std::size_t position = 0; position < LENGTH; ++position) {
			const auto high = DecodeNibble_(text[position * 2]);
			const auto low = DecodeNibble_(text[position * 2 + 1]);

			if (!high.has_value() || !low.has_value()) {
				return std::unexpected(FixedBytesError::IllegalCharacter);
			}

			result._bytes[position] = static_cast<std::uint8_t>((*high << 4) | *low);
		}
		return result;
	}

	[[nodiscard]] std::string ToHex() const {
		static constexpr std::string_view DIGITS = "0123456789abcdef";

		std::string text;
		text.reserve(HEX_LENGTH);
		for (const std::uint8_t value : _bytes) {
			text.push_back(DIGITS[value >> 4]);
			text.push_back(DIGITS[value & 0x0F]);
		}
		return text;
	}

	[[nodiscard]] std::span<const std::uint8_t> Bytes() const noexcept {
		return std::span<const std::uint8_t>(_bytes.data(), LENGTH);
	}

	[[nodiscard]] const std::uint8_t* Data() const noexcept {
		return _bytes.data();
	}

	[[nodiscard]] std::uint8_t* Data() noexcept {
		return _bytes.data();
	}

	bool operator==(const FixedBytes& other) const noexcept = default;

private:
	static constexpr std::expected<std::uint8_t, FixedBytesError> DecodeNibble_(const char character) {
		if (character >= '0' && character <= '9') {
			return static_cast<std::uint8_t>(character - '0');
		}
		if (character >= 'a' && character <= 'f') {
			return static_cast<std::uint8_t>(character - 'a' + 10);
		}
		return std::unexpected(FixedBytesError::IllegalCharacter);
	}

	std::array<std::uint8_t, LENGTH> _bytes;
};
}
