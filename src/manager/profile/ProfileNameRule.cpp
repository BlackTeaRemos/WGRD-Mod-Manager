#include "manager/profile/ProfileNameRule.h"

#include <algorithm>

namespace wgrd::manager {
namespace {
	bool IsAcceptableCharacter(const char character) {
		const bool upper = character >= 'A' && character <= 'Z';
		const bool lower = character >= 'a' && character <= 'z';
		const bool digit = character >= '0' && character <= '9';

		return upper || lower || digit || character == '.' || character == '_' || character == '-';
	}
}

bool ProfileNameRule::Accepts(std::string_view name) {
	if (name.empty() || name.size() > LENGTH_LIMIT) {
		return false;
	}

	return std::ranges::all_of(name, IsAcceptableCharacter);
}
}
