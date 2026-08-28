#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace wgrd::domain {
enum class ModIdentifierError {
	Empty
	, IllegalCharacter
	, LeadingHyphen
	, TrailingHyphen
};

class ModIdentifier {
public:
	static std::expected<ModIdentifier, ModIdentifierError> Parse(std::string_view text);

	[[nodiscard]] const std::string& Value() const noexcept;

	bool operator==(const ModIdentifier& other) const noexcept = default;

private:
	explicit ModIdentifier(std::string value);

	static bool IsLegalCharacter_(char character) noexcept;

	std::string _value;
};
}
