#include "gui/screens/OrderScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/OrderText.h"

#include <format>
#include <string>

namespace wgrd::gui {
const domain::Annotation* OrderScreen::AnnotationFor_(
	const domain::OrderSnapshot& snapshot,
	const domain::InstallFolder& folder
) {
	for (const domain::Annotation& annotation : snapshot.annotations) {
		if (annotation.folder == folder) {
			return &annotation;
		}
	}

	return nullptr;
}

bool OrderScreen::Unverified_(
	domain::ICatalogService* catalogService,
	const domain::InstallFolder& folder
) {
	if (catalogService == nullptr) {
		return false;
	}

	return !catalogService->Verified(folder.Value());
}

bool OrderScreen::DrawEntries_(
	const ScreenArea& area,
	const float cursorY,
	const float rightX,
	const domain::OrderSnapshot& snapshot,
	domain::IOrderService* orderService,
	domain::ICatalogService* catalogService
) {
	const float leftWidth = rightX - area.origin.x;

	design::HeadingBar(
		ImVec2(area.origin.x, cursorY),
		leftWidth,
		text::order::HEADING,
		design::HeadingLevel::Section
	);

	float leftY = cursorY + design::HeadingHeight(design::HeadingLevel::Section);

	std::size_t index = 0;
	for (const domain::OrderEntryView& entry : snapshot.entries) {
		const ImVec2 rowTopLeft(area.origin.x, leftY);
		const ImVec2 rowBottomRight(rightX, leftY + ROW_HEIGHT);

		const bool blocking = entry.enabled && !entry.present;
		const bool selected = entry.folder.Value() == _selected;

		bool hovered = false;
		const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);

		if (blocking) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_WARNED_ROW);
			design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::FAILURE);
		} else if (selected) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_ACTIVE_FILL);
		} else if (hovered) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
		}

		if (selected) {
			design::FillRect(rowTopLeft, ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y), tokens::ACCENT);
		}

		if (clicked) {
			_selected = entry.folder.Value();
		}

		const std::string position = std::format(text::common::POSITION_FORMAT, index + 1);

		design::TextAt(ImVec2(rowTopLeft.x + 8.0f, leftY + 8.0f), text::order::DRAG_HANDLE, tokens::TEXT_DISABLED, 11.0f);
		design::TextAt(ImVec2(rowTopLeft.x + 22.0f, leftY + 8.0f), position, tokens::TEXT_DISABLED, 11.0f);

		if (design::Checkbox(ImVec2(rowTopLeft.x + 42.0f, leftY + 6.0f), entry.enabled)) {
			orderService->SetEnabled(entry.folder, !entry.enabled);
			return true;
		}

		const ImU32 nameColor = entry.enabled ? tokens::SECONDARY : tokens::TEXT_DISABLED;
		design::TextAt(ImVec2(rowTopLeft.x + 62.0f, leftY + 4.0f), entry.folder.Value(), nameColor, 12.0f);

		if (Unverified_(catalogService, entry.folder)) {
			const float nameWidth = design::MeasureText(entry.folder.Value(), 12.0f).x;

			design::TextAt(
				ImVec2(rowTopLeft.x + 68.0f + nameWidth, leftY + 6.0f),
				text::order::UNSIGNED,
				tokens::FAILURE,
				9.0f
			);
		}

		const std::string location = snapshot.modsDirectory.empty()
		                             ? entry.folder.Value()
		                             : std::format(text::order::LOCATION_FORMAT, snapshot.modsDirectory, entry.folder.Value());

		design::TextAt(ImVec2(rowTopLeft.x + 62.0f, leftY + 17.0f), location, tokens::SECONDARY_MUTED, 10.0f);

		if (index > 0) {
			if (design::Button(
				ImVec2(rowBottomRight.x - 54.0f, leftY + 6.0f),
				text::order::MOVE_UP,
				design::ButtonVariant::Neutral
			)) {
				orderService->Move(index, index - 1);
				return true;
			}
		}

		if (index + 1 < snapshot.entries.size()) {
			if (design::Button(
				ImVec2(rowBottomRight.x - 28.0f, leftY + 6.0f),
				text::order::MOVE_DOWN,
				design::ButtonVariant::Neutral
			)) {
				orderService->Move(index, index + 1);
				return true;
			}
		}

		if (blocking) {
			const domain::Annotation* const reason = AnnotationFor_(snapshot, entry.folder);

			if (reason != nullptr) {
				design::TextAt(
					ImVec2(rowTopLeft.x + 62.0f, leftY + 28.0f),
					reason->tag,
					tokens::FAILURE,
					10.0f
				);
				design::TextAt(
					ImVec2(rowTopLeft.x + 110.0f, leftY + 28.0f),
					reason->explanation,
					tokens::FAILURE,
					10.0f
				);
			}
		}

		design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

		leftY += ROW_HEIGHT;
		++index;
	}

	return false;
}

const domain::InstalledMod* OrderScreen::InstalledFor_(
	const domain::OrderSnapshot& snapshot,
	const domain::InstallFolder& folder
) {
	for (const domain::InstalledMod& mod : snapshot.installed) {
		if (mod.folder == folder) {
			return &mod;
		}
	}

	return nullptr;
}

float OrderScreen::DrawDetails_(
	const float left,
	float cursorY,
	const float width,
	const domain::OrderSnapshot& snapshot
) const {
	if (_selected.empty()) {
		design::TextAt(ImVec2(left, cursorY + 6.0f), text::order::NOTHING_SELECTED, tokens::TEXT_DISABLED, 10.0f);
		return cursorY + 24.0f;
	}

	const auto folder = domain::InstallFolder::Parse(_selected);
	const domain::InstalledMod* const mod = folder.has_value()
	                                        ? InstalledFor_(snapshot, *folder)
	                                        : nullptr;

	design::TextAt(ImVec2(left, cursorY + 6.0f), _selected, tokens::ACCENT, 12.0f);
	cursorY += 22.0f;

	if (mod == nullptr) {
		design::TextAt(ImVec2(left, cursorY + 4.0f), text::order::NOT_INSTALLED, tokens::FAILURE, 10.0f);
		return cursorY + 22.0f;
	}

	const domain::ModMetadata& metadata = mod->metadata;

	if (metadata.present && !metadata.description.empty()) {
		cursorY = design::WrappedTextAt(
			ImVec2(left, cursorY + 2.0f),
			width,
			metadata.description,
			tokens::SECONDARY,
			10.0f
		);
		cursorY += 6.0f;
	}

	const auto field = [&](const std::string_view label, const std::string& value) {
		if (value.empty()) {
			return;
		}

		design::TextAt(ImVec2(left, cursorY + 3.0f), label, tokens::SECONDARY_MUTED, 9.0f);
		design::TextAt(ImVec2(left + 70.0f, cursorY + 3.0f), value, tokens::SECONDARY, 10.0f);
		cursorY += 15.0f;
	};

	if (metadata.present) {
		field(text::order::FIELD_VERSION, metadata.version);
		field(text::order::FIELD_AUTHOR, metadata.author);
		field(text::order::FIELD_REVISION, metadata.builtFromRevision);
		field(text::order::FIELD_PACKS, std::to_string(metadata.packCount));
	} else {
		design::TextAt(ImVec2(left, cursorY + 3.0f), text::order::NO_MANIFEST, tokens::ADVISORY, 10.0f);
		cursorY += 15.0f;
	}

	std::string builds;
	for (const domain::GameBuild& build : mod->builds) {
		builds += build.ToText();
		builds += " ";
	}

	field(text::order::FIELD_BUILDS, builds);

	return cursorY + 4.0f;
}

void OrderScreen::DrawSidebar_(
	const ScreenArea& area,
	const float cursorY,
	const float rightX,
	const domain::OrderSnapshot& snapshot
) const {
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(rightX, cursorY),
		ImVec2(rightX, area.origin.y + area.height),
		tokens::BORDER
	);

	const float rightWidth = area.origin.x + area.width - rightX;
	float rightY = cursorY;

	design::HeadingBar(
		ImVec2(rightX, rightY),
		rightWidth,
		std::format(text::order::ATTENTION_FORMAT, snapshot.annotations.size()),
		design::HeadingLevel::Section,
		design::HeadingTone::Warning
	);
	rightY += design::HeadingHeight(design::HeadingLevel::Section);

	for (const domain::Annotation& annotation : snapshot.annotations) {
		const bool severe = annotation.severity == domain::AnnotationSeverity::Blocking;
		const ImU32 tone = severe ? tokens::FAILURE : tokens::ADVISORY;

		design::TextAt(ImVec2(rightX + 6.0f, rightY + 5.0f), annotation.folder.Value(), tone, 11.0f);
		design::TextAt(ImVec2(rightX + 6.0f, rightY + 19.0f), annotation.tag, tokens::SECONDARY_MUTED, 10.0f);
		design::TextAt(
			ImVec2(rightX + 50.0f, rightY + 19.0f),
			annotation.explanation,
			tokens::SECONDARY_MUTED,
			10.0f
		);

		rightY += 36.0f;
		design::HorizontalRule(rightX, rightX + rightWidth, rightY, tokens::BORDER_SUBTLE);
	}

	if (!snapshot.annotations.empty()) {
		rightY += 8.0f;
	}

	design::HeadingBar(ImVec2(rightX, rightY), rightWidth, text::order::DETAILS, design::HeadingLevel::Minor);
	rightY += design::HeadingHeight(design::HeadingLevel::Minor);

	rightY = DrawDetails_(rightX + 6.0f, rightY, rightWidth - 12.0f, snapshot);

	rightY += 8.0f;

	design::HeadingBar(ImVec2(rightX, rightY), rightWidth, text::order::ORDER_FILE, design::HeadingLevel::Minor);
	rightY += design::HeadingHeight(design::HeadingLevel::Minor);

	design::TextAt(ImVec2(rightX + 6.0f, rightY + 4.0f), snapshot.orderFile, tokens::SECONDARY_MUTED, 10.0f);
	rightY += 16.0f;
	design::TextAt(
		ImVec2(rightX + 6.0f, rightY + 4.0f),
		snapshot.writable ? text::common::WRITABLE : text::common::READ_ONLY,
		snapshot.writable ? tokens::SUCCESS : tokens::FAILURE,
		10.0f
	);
}

float OrderScreen::DrawDefaultBanner_(
	const ScreenArea& area,
	const float cursorY,
	domain::IProfileService* profileService
) {
	if (profileService == nullptr || profileService->Active() != domain::DEFAULT_PROFILE_NAME) {
		return cursorY;
	}

	const ImVec2 topLeft(area.origin.x, cursorY);
	const ImVec2 bottomRight(area.origin.x + area.width, cursorY + BANNER_HEIGHT);

	design::FillRect(topLeft, bottomRight, tokens::ADVISORY_SURFACE);
	design::FillRect(topLeft, ImVec2(topLeft.x + 3.0f, bottomRight.y), tokens::ADVISORY);

	design::TextAt(
		ImVec2(topLeft.x + 12.0f, cursorY + 6.0f),
		text::order::DEFAULT_BANNER,
		tokens::WARNING_HEADING,
		11.0f
	);

	design::TextAt(
		ImVec2(topLeft.x + 12.0f, cursorY + 20.0f),
		text::order::DEFAULT_BANNER_DETAIL,
		tokens::ADVISORY,
		10.0f
	);

	design::TextAt(
		ImVec2(topLeft.x + 180.0f, cursorY + 20.0f),
		text::order::DEFAULT_BANNER_ADVICE,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	design::HorizontalRule(topLeft.x, bottomRight.x, bottomRight.y, tokens::BORDER);

	return bottomRight.y + 12.0f;
}

void OrderScreen::Draw(
	const ScreenArea& area,
	domain::IOrderService* orderService,
	domain::IProfileService* profileService,
	domain::ICatalogService* catalogService
) {
	if (orderService == nullptr) {
		const float cursorY = ScreenToolbar::Draw(area, text::order::TITLE, text::common::GAME_MISSING);
		design::TextAt(
			ImVec2(area.origin.x + 6.0f, cursorY + 16.0f),
			text::common::NO_INSTALLATION,
			tokens::FAILURE,
			12.0f
		);
		return;
	}

	const domain::OrderSnapshot& snapshot = orderService->Current();

	const std::string context = std::format(
		text::order::CONTEXT_FORMAT,
		snapshot.entries.size(),
		snapshot.enabledCount
	);

	float cursorY = ScreenToolbar::Draw(area, text::order::TITLE, context);

	cursorY = DrawDefaultBanner_(area, cursorY, profileService);

	const float rightX = area.origin.x + area.width * 0.6f;

	if (DrawEntries_(area, cursorY, rightX, snapshot, orderService, catalogService)) {
		return;
	}

	DrawSidebar_(area, cursorY, rightX, snapshot);
}
}
