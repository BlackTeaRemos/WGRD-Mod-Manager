#include "domain/rules/ModNameRule.h"

#include <algorithm>

namespace wgrd::domain {
namespace {
	bool IsAcceptableCharacter(const char character) {
		const bool upper = character >= 'A' && character <= 'Z';
		const bool lower = character >= 'a' && character <= 'z';
		const bool digit = character >= '0' && character <= '9';

		return upper || lower || digit || character == '_' || character == '-';
	}
}

bool ModNameRule::IsAcceptable(const std::string_view modName) {
	if (modName.empty() || modName.size() > MAXIMUM_LENGTH) {
		return false;
	}

	return std::ranges::all_of(modName, IsAcceptableCharacter);
}
}
