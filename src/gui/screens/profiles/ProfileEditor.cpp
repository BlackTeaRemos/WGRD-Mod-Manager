#include "gui/screens/profiles/ProfilesScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/ProfilesText.h"

#include "gui/design/text/ClippedText.h"

#include <optional>
#include <algorithm>
#include <format>
#include <vector>

namespace wgrd::gui {
namespace {
	constexpr float DETAIL_ROW_HEIGHT = 20.0f;
}

void ProfilesScreen::DrawDetail_(
	const ScreenArea& area,
	float cursorY,
	ApplicationState& state,
	domain::IProfileService* profiles,
	domain::IOrderService* order
) {
	const float left = area.origin.x + (area.width < 950.0f ? 6.0f : LIST_WIDTH + 12.0f);
	const float width = area.width - (area.width < 950.0f ? 12.0f : LIST_WIDTH + 18.0f);

	if (area.width >= 950.0f) {
		ImGui::GetWindowDrawList()->AddLine(ImVec2(area.origin.x + LIST_WIDTH, cursorY),
			ImVec2(area.origin.x + LIST_WIDTH, area.origin.y + area.height), tokens::BORDER);
	}

	design::HeadingBar(
		HeadingOrigin_(area, cursorY),
		HeadingWidth_(area),
		text::profiles::CAPTURE_LABEL,
		design::HeadingLevel::Minor
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor) + 4.0f;

	const ImVec2 captureSize = design::ButtonSize(
		text::profiles::CAPTURE,
		11.0f,
		design::FIELD_HEIGHT
	);

	design::TextField(
		ImVec2(left, cursorY),
		width - captureSize.x - 6.0f,
		text::profiles::NAME_FIELD,
		_nameBuffer.data(),
		_nameBuffer.size()
	);

	if (design::Button(
		ImVec2(left + width - captureSize.x, cursorY),
		text::profiles::CAPTURE,
		design::ButtonVariant::Accent,
		true,
		11.0f,
		true,
		design::FIELD_HEIGHT
	)) {
		_operationFailed = false;

		const auto captured = profiles->CaptureCurrent(_nameBuffer.data());
		_operationFailed = !captured.has_value();
		if (captured.has_value()) {
			_nameBuffer.fill('\0');
		}
	}

	cursorY += design::FIELD_HEIGHT + 8.0f;

	const auto* selectedRow = Selected_(state, profiles);
	const std::optional<domain::ProfileSummary> selection = selectedRow == nullptr ? std::nullopt : std::optional<domain::ProfileSummary>(*selectedRow);
	const auto* selected = selection.has_value() ? &*selection : nullptr;

	if (selected == nullptr) {
		design::TextAt(
			ImVec2(left, cursorY),
			profiles->LastMessage(),
			_operationFailed ? tokens::FAILURE : tokens::SECONDARY_MUTED,
			10.0f, FontRole::Body
		);

		(void)DrawGameProfiles_(area, cursorY + 18.0f, profiles);
		return;
	}

	design::HeadingBar(
		HeadingOrigin_(area, cursorY),
		HeadingWidth_(area),
		selected->name,
		design::HeadingLevel::Minor,
		selected->active ? design::HeadingTone::Success : design::HeadingTone::Accent
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor) + 4.0f;

	const std::string_view activation = selected->active ? text::profiles::FORCE_SELECT : text::profiles::SELECT;
	const float cloneLeft = left + design::ButtonSize(activation, 11.0f).x + 6.0f;
	const float removeLeft = cloneLeft + design::ButtonSize(text::profiles::CLONE, 11.0f).x + 6.0f;
	if (design::Button(
		ImVec2(left, cursorY),
		activation,
		design::ButtonVariant::Accent,
		true,
		11.0f
	)) {
		_operationFailed = false;

		const auto activated = profiles->Activate(selected->name);
		_operationFailed = !activated.has_value();
		if (activated.has_value() && order != nullptr) {
			order->Refresh();
		}
	}

	if (design::Button(
		ImVec2(cloneLeft, cursorY),
		text::profiles::CLONE,
		design::ButtonVariant::Neutral,
		false,
		11.0f
	)) {
		_operationFailed = false;

		const auto cloned = profiles->Clone(selected->name, _nameBuffer.data());
		_operationFailed = !cloned.has_value();
		if (cloned.has_value()) {
			_nameBuffer.fill('\0');
		}
	}

	const bool removable = selected->name != domain::DEFAULT_PROFILE_NAME;

	if (design::Button(
		ImVec2(removeLeft, cursorY),
		text::profiles::DELETE_PROFILE,
		design::ButtonVariant::Failure,
		false,
		11.0f,
		removable
	)) {
		_operationFailed = false;

		const auto removed = profiles->Remove(selected->name);
		_operationFailed = !removed.has_value();
		if (removed.has_value()) {
			state.SelectProfile(0);
			return;
		}
	}

	cursorY += 26.0f;

	design::ClippedText::Draw(ImVec2(left, cursorY), width - 110.0f, profiles->LastMessage(), _operationFailed ? tokens::FAILURE : tokens::SECONDARY_MUTED, 10.0f, FontRole::Body);
	if (_operationFailed && design::Button(ImVec2(left + width - 100.0f, cursorY), "Copy error", design::ButtonVariant::Neutral)) {
		ImGui::SetClipboardText(profiles->LastMessage().c_str());
	}
	cursorY += 24.0f;

	if (selected->missingCount > 0) {
		design::TextAt(
			ImVec2(left, cursorY),
			std::format(text::profiles::MISSING_FORMAT, selected->missingCount),
			tokens::FAILURE,
			10.0f
		);
		cursorY += 18.0f;
	}

	design::TextAt(
		ImVec2(left, cursorY),
		selected->gameProfileHeld
		? text::profiles::PROFILE_FILE_HELD
		: text::profiles::PROFILE_FILE_ABSENT,
		selected->gameProfileHeld ? tokens::SUCCESS : tokens::TEXT_DISABLED,
		10.0f
	);

	if (!selected->account.empty()) {
		design::TextAt(
			ImVec2(left + 110.0f, cursorY),
			std::format(text::profiles::ACCOUNT_FORMAT, selected->account),
			tokens::SECONDARY_MUTED,
			10.0f
		);
	}

	cursorY += 20.0f;

	cursorY = DrawGameProfiles_(area, cursorY, profiles);

	design::HeadingBar(
		HeadingOrigin_(area, cursorY),
		HeadingWidth_(area),
		text::profiles::LOAD_ORDER,
		design::HeadingLevel::Minor
	);

	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	const auto profile = profiles->Load(selected->name);
	if (!profile.has_value()) {
		design::TextAt(ImVec2(left, cursorY + 4.0f), text::profiles::UNREADABLE, tokens::FAILURE, 10.0f);
		return;
	}

	static const std::vector<domain::InstalledMod> NOTHING_INSTALLED;

	const std::vector<domain::InstalledMod>& installed = order != nullptr
	                                                     ? order->Current().installed
	                                                     : NOTHING_INSTALLED;

	std::size_t position = 0;

	for (const domain::OrderEntry& entry : profile->Order().Entries()) {
		if (!entry.enabled) {
			continue;
		}

		++position;

		const bool present = std::ranges::any_of(installed, [&](const domain::InstalledMod& mod) {
				return mod.folder == entry.folder;
			}
		);

		design::TextAt(
			ImVec2(left, cursorY + 4.0f),
			std::format(text::profiles::POSITION_FORMAT, position),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::TextAt(
			ImVec2(left + 28.0f, cursorY + 4.0f),
			entry.folder.Value(),
			present ? tokens::SECONDARY : tokens::FAILURE,
			11.0f, FontRole::Name
		);

		if (!present) {
			design::TextAt(
				ImVec2(left + 240.0f, cursorY + 4.0f),
				text::profiles::NOT_INSTALLED,
				tokens::FAILURE,
				10.0f
			);
		}

		cursorY += DETAIL_ROW_HEIGHT;
		design::HorizontalRule(left, left + width, cursorY, tokens::BORDER_SUBTLE);
	}

	if (position == 0) {
		design::TextAt(ImVec2(left, cursorY + 4.0f), text::profiles::NOTHING_ENABLED, tokens::TEXT_DISABLED, 10.0f);
	}
}

}
