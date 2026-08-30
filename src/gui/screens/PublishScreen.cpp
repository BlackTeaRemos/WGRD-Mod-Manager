#include "gui/screens/PublishScreen.h"

#include "domain/rules/PublisherNameRule.h"
#include "gui/design/PassphraseMeter.h"
#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/PublishText.h"

#include <format>
#include <string>

namespace wgrd::gui {
namespace {
	constexpr std::array<std::string_view, PublishScreen::STEP_COUNT> STEPS = {
		text::publish::STEP_CREATE, text::publish::STEP_SOURCE, text::publish::STEP_SIGN
	};

	constexpr float STEP_BAR_HEIGHT = 22.0f;
	constexpr float CANDIDATE_ROW_HEIGHT = 22.0f;
	constexpr float FIELD_WIDTH_LIMIT = 420.0f;

	constexpr std::string_view KEY_FILTER_LABEL = text::publish::KEY_FILTER_LABEL;
	constexpr std::string_view KEY_FILTER_PATTERN = text::publish::KEY_FILTER_PATTERN;
	constexpr std::string_view KEY_SUGGESTED_NAME = text::publish::KEY_SUGGESTED_NAME;

	std::string Shorten(const std::filesystem::path& path, const std::size_t limit) {
		const std::string text = path.string();
		if (text.size() <= limit) {
			return text;
		}

		return std::string(text::common::ELLIPSIS) + text.substr(text.size() - limit);
	}
}

void PublishScreen::ClearSecrets_() {
	_createPassphrase.fill('\0');
	_unlockPassphrase.fill('\0');
}

std::string_view PublishScreen::Notice_(domain::IPublishService* publish) const {
	if (!_notice.empty()) {
		return _notice;
	}

	return publish->LastMessage();
}

bool PublishScreen::StepSatisfied_(
	const std::size_t index,
	const ApplicationState& state,
	domain::IPublishService* publish
) {
	const bool keyReady = publish != nullptr && publish->Publisher().present;
	const bool folderReady = !state.PublishFolder().empty();

	switch (index) {
		case 0:
			return keyReady;
		case 1:
			return folderReady;
		default:
			break;
	}

	return keyReady && folderReady;
}

float PublishScreen::DrawStepBar_(
	const ScreenArea& area,
	const float cursorY,
	ApplicationState& state,
	domain::IPublishService* publish
) const {
	const float stepWidth = area.width / static_cast<float>(STEPS.size());

	for (std::size_t index = 0; index < STEPS.size(); ++index) {
		const ImVec2 stepTopLeft(area.origin.x + stepWidth * static_cast<float>(index), cursorY);
		const ImVec2 stepBottomRight(stepTopLeft.x + stepWidth, cursorY + STEP_BAR_HEIGHT);

		bool hovered = false;
		const bool clicked = design::RowHit(stepTopLeft, stepBottomRight, hovered);
		const bool current = state.PublishStep() == index;
		const bool satisfied = StepSatisfied_(index, state, publish);
		const bool visited = index < state.PublishStep();

		if (current) {
			design::FillRect(stepTopLeft, stepBottomRight, tokens::ACCENT_ACTIVE_FILL);
		}

		ImU32 color = tokens::TEXT_DISABLED;

		if (current) {
			color = tokens::ACCENT;
		} else if (satisfied) {
			color = tokens::SUCCESS;
		} else if (visited) {
			color = tokens::FAILURE;
		} else if (hovered) {
			color = tokens::ACCENT_HOVER;
		}

		const float extent = design::MeasureText(STEPS[index], 10.0f).x;
		design::TextAt(
			ImVec2(stepTopLeft.x + (stepWidth - extent) * 0.5f, cursorY + 7.0f),
			STEPS[index],
			color,
			10.0f
		);

		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(stepBottomRight.x, cursorY),
			ImVec2(stepBottomRight.x, stepBottomRight.y),
			tokens::BORDER
		);

		if (clicked) {
			state.SetPublishStep(index);
		}
	}

	const float bottom = cursorY + STEP_BAR_HEIGHT;
	design::HorizontalRule(area.origin.x, area.origin.x + area.width, bottom, tokens::BORDER);

	return bottom + 10.0f;
}

float PublishScreen::DrawKeyStep_(
	const ScreenArea& area,
	float cursorY,
	domain::IPublishService* publish,
	const domain::IFilePicker* files
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
		_notice.clear();

		if (files == nullptr) {
			_notice = text::publish::PICKER_UNAVAILABLE;
		} else {
			const auto chosen = files->SaveFile(
				text::publish::PICKER_STORE_KEY,
				KEY_SUGGESTED_NAME,
				KEY_FILTER_LABEL,
				KEY_FILTER_PATTERN
			);

			if (!chosen.has_value()) {
				_notice = text::publish::NO_LOCATION;
			} else {
				const auto created = publish->CreateKey(
					_nameBuffer.data(),
					*chosen,
					_createPassphrase.data()
				);

				if (created.has_value()) {
					_nameBuffer.fill('\0');
					ClearSecrets_();
				}
			}
		}
	}

	cursorY += 28.0f;

	design::TextAt(
		ImVec2(left, cursorY),
		Notice_(publish),
		_notice.empty() ? tokens::SECONDARY : tokens::WARNING_HEADING,
		10.0f
	);
	return cursorY + 22.0f;
}

float PublishScreen::DrawSourceStep_(
	const ScreenArea& area,
	float cursorY,
	ApplicationState& state,
	domain::IPublishService* publish
) const {
	design::TextAt(ImVec2(area.origin.x + 6.0f, cursorY), text::publish::MOD_FOLDER, tokens::SECONDARY_MUTED, 10.0f);
	cursorY += 16.0f;

	const std::vector<std::string>& candidates = publish->Candidates();

	if (candidates.empty()) {
		ScreenToolbar::Placeholder(area, cursorY, text::publish::NO_CANDIDATES);
		return cursorY + 34.0f;
	}

	for (const std::string& candidate : candidates) {
		const ImVec2 rowTopLeft(area.origin.x, cursorY);
		const ImVec2 rowBottomRight(area.origin.x + area.width, cursorY + CANDIDATE_ROW_HEIGHT);

		bool hovered = false;
		const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);
		const bool selected = state.PublishFolder() == candidate;

		if (selected) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_ACTIVE_FILL);
			design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::ACCENT);
		} else if (hovered) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
		}

		design::TextAt(
			ImVec2(rowTopLeft.x + 8.0f, cursorY + 5.0f),
			candidate,
			selected ? tokens::ACCENT : tokens::SECONDARY,
			11.0f
		);

		design::TextAt(
			ImVec2(rowTopLeft.x + 300.0f, cursorY + 5.0f),
			std::format(text::publish::CANDIDATE_LOCATION_FORMAT, candidate),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

		if (clicked) {
			state.SelectPublishFolder(candidate);
		}

		cursorY += CANDIDATE_ROW_HEIGHT;
	}

	return cursorY + 8.0f;
}

float PublishScreen::DrawSignStep_(
	const ScreenArea& area,
	float cursorY,
	ApplicationState& state,
	domain::IPublishService* publish,
	domain::ICatalogService* catalog,
	const domain::IFilePicker* files
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

	if (keyReady) {
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
			ClearSecrets_();
		}

		cursorY += 30.0f;
	} else {
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
			_notice.clear();

			if (files == nullptr) {
				_notice = text::publish::PICKER_UNAVAILABLE;
			} else {
				const auto chosen = files->OpenFile(
					text::publish::PICKER_SELECT_KEY,
					KEY_FILTER_LABEL,
					KEY_FILTER_PATTERN
				);

				if (!chosen.has_value()) {
					_notice = text::publish::NO_KEY_FILE;
				} else {
					const auto unlocked = publish->UnlockKey(*chosen, _unlockPassphrase.data());
					if (unlocked.has_value()) {
						ClearSecrets_();
					}
				}
			}
		}

		cursorY += 30.0f;
	}

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
			_notice.clear();
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
		Notice_(publish),
		_notice.empty() ? tokens::SECONDARY_MUTED : tokens::WARNING_HEADING,
		10.0f
	);

	return cursorY + 22.0f;
}

void PublishScreen::DrawHistory_(
	const ScreenArea& area,
	const float cursorY,
	domain::IPublishService* publish
) const {
	design::HeadingBar(
		ImVec2(area.origin.x, cursorY),
		area.width,
		text::publish::HISTORY,
		design::HeadingLevel::Minor
	);

	float rowY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);

	const std::vector<domain::PublishedRelease>& history = publish->History();

	if (history.empty()) {
		ScreenToolbar::Placeholder(area, rowY, text::publish::HISTORY_EMPTY);
		return;
	}

	for (const domain::PublishedRelease& release : history) {
		design::TextAt(ImVec2(area.origin.x + 6.0f, rowY + 4.0f), release.modName, tokens::SECONDARY, 11.0f);
		design::TextAt(
			ImVec2(area.origin.x + 200.0f, rowY + 4.0f),
			std::format(text::common::VERSION_FORMAT, release.version),
			tokens::SECONDARY_MUTED,
			10.0f
		);
		design::TextAt(
			ImVec2(area.origin.x + 260.0f, rowY + 4.0f),
			ScreenToolbar::FormatBytes(release.totalBytes),
			tokens::SECONDARY,
			10.0f
		);
		design::TextAt(
			ImVec2(area.origin.x + 360.0f, rowY + 4.0f),
			std::format(text::publish::CHUNKS_FILES_FORMAT, release.chunkCount, release.fileCount),
			tokens::SECONDARY_MUTED,
			10.0f
		);
		design::TextAt(
			ImVec2(area.origin.x + 560.0f, rowY + 4.0f),
			release.manifestDigest.substr(0, 16),
			tokens::ACCENT,
			10.0f
		);

		rowY += 20.0f;
		design::HorizontalRule(area.origin.x, area.origin.x + area.width, rowY, tokens::BORDER_SUBTLE);
	}
}

void PublishScreen::Draw(
	const ScreenArea& area,
	ApplicationState& state,
	domain::IPublishService* publish,
	domain::ICatalogService* catalog,
	const domain::IFilePicker* files
) {
	if (publish == nullptr) {
		const float cursorY = ScreenToolbar::Draw(area, text::publish::TITLE, text::publish::UNAVAILABLE);
		ScreenToolbar::Placeholder(area, cursorY, text::common::NO_INSTALLATION);
		return;
	}

	const domain::PublisherState& publisher = publish->Publisher();

	const std::string context = publisher.present
	                            ? std::format(text::publish::PUBLISHING_AS_FORMAT, publisher.fingerprint)
	                            : std::string(text::publish::NO_KEY);

	float cursorY = ScreenToolbar::Draw(area, text::publish::TITLE, context);

	cursorY = DrawStepBar_(area, cursorY, state, publish);

	const std::size_t step = state.PublishStep() < STEP_COUNT ? state.PublishStep() : 0;

	if (step == 0) {
		cursorY = DrawKeyStep_(area, cursorY, publish, files);
	} else if (step == 1) {
		cursorY = DrawSourceStep_(area, cursorY, state, publish);
	} else {
		cursorY = DrawSignStep_(area, cursorY, state, publish, catalog, files);
	}

	DrawHistory_(area, cursorY + 8.0f, publish);
}
}
