#include "gui/screens/PublishSignStep.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/PublishText.h"

#include <array>
#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

namespace wgrd::gui {
namespace {
	constexpr float FIELD_WIDTH_LIMIT = 420.0f;

	std::string Shorten(const std::filesystem::path& path, const std::size_t limit) {
		const std::string text = path.string();
		if (text.size() <= limit) {
			return text;
		}

		return std::string(text::common::ELLIPSIS) + text.substr(text.size() - limit);
	}
}

void PublishSignStep::ClearSecret() {
	_unlockPassphrase.fill('\0');
}

float PublishSignStep::DrawKeyPanel_(
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

	if (publish->Publisher().present) {
		design::TextAt(ImVec2(left, cursorY), text::publish::UNLOCKED, tokens::SUCCESS, 10.0f);
		design::TextAt(
			ImVec2(left + 90.0f, cursorY),
			publish->Publisher().fingerprint,
			tokens::ACCENT,
			11.0f
		);

		cursorY += 16.0f;

		design::TextAt(
			ImVec2(left, cursorY),
			Shorten(publish->KeyPath(), 72),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		cursorY += 22.0f;

		if (design::Button(
			ImVec2(left, cursorY),
			text::publish::LOCK_KEY,
			design::ButtonVariant::Neutral,
			false,
			12.0f
		)) {
			publish->LockKey();
			clearSecrets();
		}

		return cursorY + 30.0f;
	}

	design::TextAt(ImVec2(left, cursorY), text::publish::PASSPHRASE, tokens::SECONDARY_MUTED, 10.0f);
	cursorY += 14.0f;

	design::PasswordField(
		ImVec2(left, cursorY),
		fieldWidth,
		text::publish::UNLOCK_PASSPHRASE_FIELD,
		_unlockPassphrase.data(),
		_unlockPassphrase.size()
	);

	cursorY += 30.0f;

	const bool passphraseEntered = _unlockPassphrase.front() != '\0';

	if (design::Button(
		ImVec2(left, cursorY),
		text::publish::UNLOCK_KEY,
		design::ButtonVariant::Accent,
		passphraseEntered,
		12.0f,
		passphraseEntered
	)) {
		notice.clear();

		if (files == nullptr) {
			notice = text::publish::PICKER_UNAVAILABLE;
		} else {
			const auto chosen = files->OpenFile(
				text::publish::PICKER_SELECT_KEY,
				text::publish::KEY_FILTER_LABEL,
				text::publish::KEY_FILTER_PATTERN
			);

			if (!chosen.has_value()) {
				notice = text::publish::NO_KEY_FILE;
			} else {
				const auto unlocked = publish->UnlockKey(*chosen, _unlockPassphrase.data());
				if (unlocked.has_value()) {
					clearSecrets();
				}
			}
		}
	}

	return cursorY + 30.0f;
}

float PublishSignStep::Draw(
	const ScreenArea& area,
	float cursorY,
	ApplicationState& state,
	domain::IPublishService* publish,
	domain::ICatalogService* catalog,
	const domain::IFilePicker* files,
	std::string& notice,
	const std::function<void()>& clearSecrets
) {
	const float left = area.origin.x + 6.0f;
	const float fieldWidth = area.width - 12.0f < FIELD_WIDTH_LIMIT
	                         ? area.width - 12.0f
	                         : FIELD_WIDTH_LIMIT;

	const bool keyReady = publish->Publisher().present;
	const bool folderReady = !state.PublishFolder().empty();

	design::HeadingBar(
		ImVec2(area.origin.x, cursorY),
		area.width,
		text::publish::SIGNING_KEY,
		design::HeadingLevel::Minor,
		keyReady ? design::HeadingTone::Success : design::HeadingTone::Warning
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor) + 6.0f;

	cursorY = DrawKeyPanel_(area, cursorY, publish, files, notice, clearSecrets);

	design::HeadingBar(
		ImVec2(area.origin.x, cursorY),
		area.width,
		keyReady && folderReady ? text::publish::PREFLIGHT_READY : text::publish::PREFLIGHT_INCOMPLETE,
		design::HeadingLevel::Minor,
		keyReady && folderReady ? design::HeadingTone::Success : design::HeadingTone::Warning
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	const std::array<std::pair<bool, std::string_view>, 2> checks = {
		{
			{keyReady, text::publish::CHECK_KEY}, {folderReady, text::publish::CHECK_FOLDER}
		}
	};

	for (const auto& check : checks) {
		design::TextAt(
			ImVec2(left, cursorY + 4.0f),
			check.first ? text::common::OK : text::common::NO,
			check.first ? tokens::SUCCESS : tokens::FAILURE,
			10.0f
		);
		design::TextAt(ImVec2(area.origin.x + 30.0f, cursorY + 3.0f), check.second, tokens::SECONDARY, 11.0f);

		cursorY += 18.0f;
		design::HorizontalRule(area.origin.x, area.origin.x + area.width, cursorY, tokens::BORDER_SUBTLE);
	}

	cursorY += 10.0f;

	const domain::PublishProgress progress = publish->Progress();

	if (keyReady && folderReady) {
		if (design::Button(
			ImVec2(left, cursorY),
			text::publish::SIGN_AND_ANNOUNCE,
			design::ButtonVariant::Accent,
			true,
			12.0f,
			!progress.Busy()
		)) {
			notice.clear();
			_awaitingPublish = true;

			publish->StartPublish(state.PublishFolder());
		}
		cursorY += 26.0f;
	}

	if (progress.Busy() || progress.totalBytes > 0) {
		const float barWidth = fieldWidth;

		const float share = progress.totalBytes == 0
		                    ? 0.0f
		                    : static_cast<float>(
			                    static_cast<double>(progress.processedBytes)
			                    / static_cast<double>(progress.totalBytes)
		                    );

		design::TransferBar(
			ImVec2(left, cursorY),
			barWidth,
			tokens::TRANSFER_BAR_HEIGHT,
			design::TransferSegments{share, 0.0f}
		);

		cursorY += tokens::TRANSFER_BAR_HEIGHT + 4.0f;

		design::TextAt(
			ImVec2(left, cursorY),
			std::format(
				text::publish::PROGRESS_FORMAT,
				ScreenToolbar::FormatBytes(progress.processedBytes),
				ScreenToolbar::FormatBytes(progress.totalBytes)
			),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		cursorY += 18.0f;
	}

	if (_awaitingPublish && !progress.Busy() && progress.phase != domain::PublishPhase::Idle) {
		_awaitingPublish = false;

		if (progress.phase == domain::PublishPhase::Done && catalog != nullptr) {
			catalog->Refresh();
		}
	}

	design::TextAt(
		ImVec2(left, cursorY),
		notice.empty() ? std::string_view(publish->LastMessage()) : std::string_view(notice),
		notice.empty() ? tokens::SECONDARY_MUTED : tokens::WARNING_HEADING,
		10.0f
	);

	return cursorY + 22.0f;
}
}
