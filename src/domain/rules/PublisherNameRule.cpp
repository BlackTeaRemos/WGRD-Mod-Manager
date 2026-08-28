#include "domain/rules/PublisherNameRule.h"

#include <algorithm>

namespace wgrd::domain {
namespace {
	bool IsAcceptableCharacter(const char character) {
		const bool upper = character >= 'A' && character <= 'Z';
		const bool lower = character >= 'a' && character <= 'z';
		const bool digit = character >= '0' && character <= '9';

		return upper || lower || digit || character == '.' || character == '_' || character == '-';
	}
}

bool PublisherNameRule::IsAcceptable(const std::string_view publisherName) {
	if (publisherName.empty() || publisherName.size() > MAXIMUM_LENGTH) {
		return false;
	}

	return std::ranges::all_of(publisherName, IsAcceptableCharacter);
}
}
