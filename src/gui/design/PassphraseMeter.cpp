#include "gui/design/PassphraseMeter.h"

#include "gui/design/Primitives.h"
#include "gui/text/PassphraseText.h"

#include <algorithm>
#include <array>

namespace wgrd::gui::design {
namespace {
	struct Rule {
		std::string_view label;
		bool required;
		bool satisfied;
	};

	bool AnyOf(const std::string_view value, bool (*predicate)(unsigned char)) {
		return std::ranges::any_of(value, [predicate](const char character) {
				return predicate(static_cast<unsigned char>(character));
			}
		);
	}

	bool IsLetter(const unsigned char character) {
		return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
	}

	bool IsDigit(const unsigned char character) {
		return character >= '0' && character <= '9';
	}

	bool IsSymbol(const unsigned char character) {
		return character > 0x20 && character < 0x7F && !IsLetter(character) && !IsDigit(character);
	}
}

float PassphraseMeter::Draw(const ImVec2 topLeft, const float width, const std::string_view passphrase) {
	const std::array<Rule, 5> rules = {
		{
			{text::passphrase::EIGHT_CHARACTERS, true, passphrase.size() >= REQUIRED_LENGTH},
			{text::passphrase::TWELVE_CHARACTERS, false, passphrase.size() >= COMFORTABLE_LENGTH},
			{text::passphrase::LETTERS, false, AnyOf(passphrase, IsLetter)},
			{text::passphrase::DIGITS, false, AnyOf(passphrase, IsDigit)},
			{text::passphrase::SYMBOLS, false, AnyOf(passphrase, IsSymbol)}
		}
	};

	const std::size_t optionalMet = static_cast<std::size_t>(
		std::ranges::count_if(rules, [](const Rule& rule) {
				return !rule.required && rule.satisfied;
			}
		)
	);

	const bool strong = optionalMet >= STRONG_OPTIONAL_COUNT;
	const bool untouched = passphrase.empty();

	float cursorX = topLeft.x;
	float cursorY = topLeft.y;

	for (const Rule& rule : rules) {
		const ImVec2 extent = MeasureText(rule.label, LABEL_SIZE);

		if (cursorX > topLeft.x && cursorX + extent.x > topLeft.x + width) {
			cursorX = topLeft.x;
			cursorY += LINE_STEP;
		}

		RuleState state = RuleState::Idle;

		if (!untouched) {
			if (rule.required) {
				if (!rule.satisfied) {
					state = RuleState::Failed;
				} else {
					state = strong ? RuleState::Passed : RuleState::Weak;
				}
			} else {
				state = rule.satisfied ? RuleState::Passed : RuleState::Weak;
			}
		}

		TextAt(ImVec2(cursorX, cursorY), rule.label, RuleTone(state), LABEL_SIZE);

		cursorX += extent.x + CHIP_GAP;
	}

	return cursorY + LINE_STEP;
}
}
