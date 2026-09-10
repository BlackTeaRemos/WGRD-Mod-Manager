#include "gui/screens/order/OrderScreen.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/text/ClippedText.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/screens/order/OrderEditToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/OrderText.h"

#include <format>
#include <string>

namespace wgrd::gui {
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
	if (_entries.Selected().empty()) {
		design::TextAt(ImVec2(left, cursorY + 6.0f), text::order::NOTHING_SELECTED, tokens::TEXT_DISABLED, 10.0f);
		return cursorY + 24.0f;
	}

	const auto folder = domain::InstallFolder::Parse(_entries.Selected());
	const domain::InstalledMod* const mod = folder.has_value()
	                                        ? InstalledFor_(snapshot, *folder)
	                                        : nullptr;

	design::TextAt(ImVec2(left, cursorY + 6.0f), _entries.Selected(), tokens::ACCENT, 12.0f, FontRole::Name);
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

	design::ClippedText::Draw(ImVec2(rightX + 6.0f, rightY + 4.0f), rightWidth - 12.0f, snapshot.orderFile, tokens::SECONDARY_MUTED, 10.0f);
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

void OrderScreen::Draw(const ScreenArea& area, const ApplicationServices& services) {
	auto* orderService = services.order;
	auto* profileService = services.profiles;
	auto* catalogService = services.catalog;
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
	cursorY = _toolbar.Draw(area, cursorY, services);

	const bool stacked = area.width < 900.0f;
	const float rightX = area.origin.x + area.width * (stacked ? 1.0f : 0.6f);

	if (_entries.Draw(area, cursorY, area.height - (cursorY - area.origin.y), rightX, snapshot, orderService, catalogService)) {
		return;
	}

	DrawSidebar_(area, stacked ? _entries.Bottom() + 12.0f : cursorY, stacked ? area.origin.x : rightX, snapshot);
}
}
