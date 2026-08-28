#include "domain/types/order/ModIdentifier.h"

namespace wgrd::domain {
std::expected<ModIdentifier, ModIdentifierError> ModIdentifier::Parse(const std::string_view text) {
	if (text.empty()) {
		return std::unexpected(ModIdentifierError::Empty);
	}
	if (text.front() == '-') {
		return std::unexpected(ModIdentifierError::LeadingHyphen);
	}
	if (text.back() == '-') {
		return std::unexpected(ModIdentifierError::TrailingHyphen);
	}

	for (const char character : text) {
		if (!IsLegalCharacter_(character)) {
			return std::unexpected(ModIdentifierError::IllegalCharacter);
		}
	}

	return ModIdentifier(std::string(text));
}

const std::string& ModIdentifier::Value() const noexcept {
	return _value;
}

ModIdentifier::ModIdentifier(std::string value)
	: _value(std::move(value)) {}

bool ModIdentifier::IsLegalCharacter_(const char character) noexcept {
	const bool isLowercaseLetter = character >= 'a' && character <= 'z';
	const bool isDigit = character >= '0' && character <= '9';
	return isLowercaseLetter || isDigit || character == '-';
}
}
