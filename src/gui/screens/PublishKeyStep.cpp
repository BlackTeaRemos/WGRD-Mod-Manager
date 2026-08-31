#include "gui/screens/PublishKeyStep.h"

#include "domain/rules/PublisherNameRule.h"
#include "gui/design/PassphraseMeter.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/text/PublishText.h"

#include <string_view>

namespace wgrd::gui {
namespace {
	constexpr float FIELD_WIDTH_LIMIT = 420.0f;
}

void PublishKeyStep::ClearSecret() {
	_createPassphrase.fill('\0');
}

float PublishKeyStep::Draw(
	const ScreenArea& area,
	float cursorY,
	domain::IPublishService* publish,
	const domain::IFilePicker* files,
	std::string& notice,
	const std::function<void()>& clearSecrets
) {
	const float left = area.origin.x + 6.0f;
	const float fieldWidth = area.width - 12.0f < FIELD_WIDTH_LIMIT
	                         ? area.width - 12.0f
	                         : FIELD_WIDTH_LIMIT;

	design::TextAt(ImVec2(left, cursorY), text::publish::PUBLISHER_NAME, tokens::SECONDARY_MUTED, 10.0f);
	cursorY += 14.0f;

	design::TextField(
		ImVec2(left, cursorY),
		fieldWidth,
		text::publish::NAME_FIELD,
		_nameBuffer.data(),
		_nameBuffer.size()
	);

	cursorY += design::FIELD_HEIGHT + 4.0f;

	const std::string_view typedName(_nameBuffer.data());

	design::RuleState nameState = design::RuleState::Idle;

	if (!typedName.empty()) {
		nameState = domain::PublisherNameRule::IsAcceptable(typedName)
		            ? design::RuleState::Passed
		            : design::RuleState::Failed;
	}

	design::TextAt(
		ImVec2(left, cursorY),
		text::publish::NAME_RULE,
		design::RuleTone(nameState),
		10.0f
	);

	cursorY += 20.0f;

	design::TextAt(ImVec2(left, cursorY), text::publish::PASSPHRASE, tokens::SECONDARY_MUTED, 10.0f);
	cursorY += 14.0f;

	design::PasswordField(
		ImVec2(left, cursorY),
		fieldWidth,
		text::publish::CREATE_PASSPHRASE_FIELD,
		_createPassphrase.data(),
		_createPassphrase.size()
	);

	cursorY += design::FIELD_HEIGHT + 4.0f;

	cursorY = design::PassphraseMeter::Draw(
		ImVec2(left, cursorY),
		fieldWidth,
		_createPassphrase.data()
	);

	cursorY += 8.0f;

	if (design::Button(
		ImVec2(left, cursorY),
		text::publish::CREATE_KEY,
		design::ButtonVariant::Accent,
		true,
		12.0f
	)) {
		notice.clear();

		if (files == nullptr) {
			notice = text::publish::PICKER_UNAVAILABLE;
		} else {
			const auto chosen = files->SaveFile(
				text::publish::PICKER_STORE_KEY,
				text::publish::KEY_SUGGESTED_NAME,
				text::publish::KEY_FILTER_LABEL,
				text::publish::KEY_FILTER_PATTERN
			);

			if (!chosen.has_value()) {
				notice = text::publish::NO_LOCATION;
			} else {
				const auto created = publish->CreateKey(
					_nameBuffer.data(),
					*chosen,
					_createPassphrase.data()
				);

				if (created.has_value()) {
					_nameBuffer.fill('\0');
					clearSecrets();
				}
			}
		}
	}

	cursorY += 28.0f;

	design::TextAt(
		ImVec2(left, cursorY),
		notice.empty() ? std::string_view(publish->LastMessage()) : std::string_view(notice),
		notice.empty() ? tokens::SECONDARY : tokens::WARNING_HEADING,
		10.0f
	);
	return cursorY + 22.0f;
}
}
