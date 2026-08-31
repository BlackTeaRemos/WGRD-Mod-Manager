#include "gui/screens/PublishScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/PublishText.h"

#include <array>
#include <format>
#include <string>
#include <string_view>

namespace wgrd::gui {
namespace {
	constexpr std::array<std::string_view, PublishScreen::STEP_COUNT> STEPS = {
		text::publish::STEP_CREATE, text::publish::STEP_SOURCE, text::publish::STEP_SIGN
	};

	constexpr float STEP_BAR_HEIGHT = 22.0f;
	constexpr float CANDIDATE_ROW_HEIGHT = 22.0f;
}

void PublishScreen::ClearSecrets_() {
	_keyStep.ClearSecret();
	_signStep.ClearSecret();
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

	const std::function<void()> clearSecrets = [this]() {
		ClearSecrets_();
	};

	if (step == 0) {
		cursorY = _keyStep.Draw(area, cursorY, publish, files, _notice, clearSecrets);
	} else if (step == 1) {
		cursorY = DrawSourceStep_(area, cursorY, state, publish);
	} else {
		cursorY = _signStep.Draw(area, cursorY, state, publish, catalog, files, _notice, clearSecrets);
	}

	DrawHistory_(area, cursorY + 8.0f, publish);
}
}
