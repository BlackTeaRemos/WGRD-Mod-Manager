#include "gui/shell/NavigationRail.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/ShellText.h"

#include <array>
#include <format>

namespace wgrd::gui {
namespace {
	struct RailEntry {
		std::string_view key;
		std::string_view label;
		Screen screen;
	};

	constexpr std::array<RailEntry, 5> ENTRIES = {
		{
			{"1", text::shell::CATALOG, Screen::Catalog}, {"2", text::shell::LOAD_ORDER, Screen::Order}, {"3", text::shell::TRANSFERS, Screen::Transfers}
			, {"4", text::shell::PROFILES, Screen::Profiles}, {"5", text::shell::PUBLISH, Screen::Publish}
		}
	};

	constexpr float ENTRY_HEIGHT = 22.0f;
	constexpr std::string_view NO_VALUE = text::common::NONE;
}

std::string NavigationRail::BadgeFor_(const Screen screen, const ApplicationServices& services) {
	switch (screen) {
		case Screen::Catalog: {
			if (services.catalog == nullptr) {
				return std::string(NO_VALUE);
			}
			return std::format(text::common::COUNT_FORMAT, services.catalog->Rows().size());
		}

		case Screen::Order: {
			if (services.order == nullptr) {
				return std::string(NO_VALUE);
			}
			const domain::OrderSnapshot& snapshot = services.order->Current();
			return std::format(text::shell::ENABLED_OF_FORMAT, snapshot.enabledCount, snapshot.entries.size());
		}

		case Screen::Transfers: {
			std::size_t active = 0;
			if (services.seeding != nullptr) {
				active += services.seeding->Entries().size();
			}
			if (services.install != nullptr && services.install->Progress().Busy()) {
				++active;
			}
			return std::format(text::common::COUNT_FORMAT, active);
		}

		case Screen::Profiles: {
			if (services.profiles == nullptr) {
				return std::string(NO_VALUE);
			}
			const std::size_t count = services.profiles->Profiles().size();
			return count == 0 ? std::string(NO_VALUE) : std::format(text::common::COUNT_FORMAT, count);
		}

		case Screen::Publish: {
			if (services.publish == nullptr) {
				return std::string(NO_VALUE);
			}
			const std::size_t published = services.publish->History().size();
			return published == 0 ? std::string(NO_VALUE) : std::format(text::common::COUNT_FORMAT, published);
		}

	}

	return std::string(NO_VALUE);
}

std::uint64_t NavigationRail::HeldBytes_(const ApplicationServices& services) {
	if (services.catalog == nullptr) {
		return 0;
	}

	std::uint64_t held = 0;

	for (const domain::CatalogRow& row : services.catalog->Rows()) {
		if (row.installed && row.manifestHeld) {
			held += row.totalBytes;
		}
	}

	return held;
}

void NavigationRail::DrawFooter_(
	const ImVec2 origin,
	const ImVec2 bottomRight,
	const float width,
	const ApplicationServices& services
) {
	const float footerTop = bottomRight.y - 52.0f;
	design::HorizontalRule(origin.x, bottomRight.x, footerTop, tokens::BORDER);

	design::TextAt(ImVec2(origin.x + 6.0f, footerTop + 6.0f), text::shell::CHUNK_STORE, tokens::SECONDARY_MUTED, 9.0f);
	design::TextAt(
		ImVec2(origin.x + 6.0f, footerTop + 18.0f),
		ScreenToolbar::FormatBytes(HeldBytes_(services)),
		tokens::SECONDARY,
		10.0f
	);

	const std::size_t seeded = services.seeding != nullptr
	                           ? services.seeding->Entries().size()
	                           : 0;

	const bool enabled = services.seeding != nullptr && services.seeding->Enabled();

	const std::string seedingText = std::format(text::shell::SEEDING_FORMAT, seeded);
	const float statusY = footerTop + 34.0f;

	design::TextAt(
		ImVec2(origin.x + 6.0f, statusY),
		seedingText,
		enabled && seeded > 0 ? tokens::SUCCESS : tokens::SECONDARY_MUTED,
		10.0f
	);

	if (services.swarm == nullptr) {
		return;
	}

	const domain::SwarmStatus& swarm = services.swarm->Status();
	const ImVec2 seedingExtent = design::MeasureText(seedingText, 10.0f);

	design::TextAt(
		ImVec2(origin.x + 6.0f + seedingExtent.x + 8.0f, statusY + 1.0f),
		std::format(text::shell::PEERS_FORMAT, swarm.dhtNodes),
		tokens::TEXT_DISABLED,
		9.0f
	);

	(void)width;
}

void NavigationRail::Draw(
	const ImVec2 origin,
	const float height,
	ApplicationState& state,
	const ApplicationServices& services
) {
	constexpr float width = tokens::RAIL_WIDTH;
	const ImVec2 bottomRight(origin.x + width, origin.y + height);

	design::FillRect(origin, bottomRight, tokens::SURFACE_RAISED);

	design::FillRect(
		origin,
		ImVec2(origin.x + width, origin.y + tokens::RAIL_HEADER_HEIGHT),
		tokens::ACCENT_RAIL_HEADER
	);
	design::TextAt(ImVec2(origin.x + 6.0f, origin.y + 3.0f), text::shell::WORKSPACE, tokens::ACCENT, 9.0f);
	design::HorizontalRule(
		origin.x,
		bottomRight.x,
		origin.y + tokens::RAIL_HEADER_HEIGHT,
		tokens::BORDER
	);

	float cursorY = origin.y + tokens::RAIL_HEADER_HEIGHT;

	for (const RailEntry& entry : ENTRIES) {
		const ImVec2 rowTopLeft(origin.x, cursorY);
		const ImVec2 rowBottomRight(origin.x + width, cursorY + ENTRY_HEIGHT);

		bool hovered = false;
		const bool clicked = design::RowHit(rowTopLeft, rowBottomRight, hovered);
		const bool active = state.ActiveScreen() == entry.screen;

		if (active) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_ACTIVE_FILL);
			design::FillRect(
				rowTopLeft,
				ImVec2(rowTopLeft.x + 2.0f, rowBottomRight.y),
				tokens::ACCENT
			);
		} else if (hovered) {
			design::FillRect(rowTopLeft, rowBottomRight, tokens::ACCENT_HOVER_FILL);
		}

		const ImU32 labelColor = active
		                         ? tokens::ACCENT
		                         : (hovered ? tokens::ACCENT_HOVER : tokens::SECONDARY);

		design::TextAt(
			ImVec2(rowTopLeft.x + 6.0f, cursorY + 6.0f),
			entry.key,
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::TextAt(ImVec2(rowTopLeft.x + 24.0f, cursorY + 5.0f), entry.label, labelColor, 12.0f);

		const std::string badge = BadgeFor_(entry.screen, services);
		const float badgeWidth = design::MeasureText(badge, 10.0f).x;
		design::TextAt(
			ImVec2(rowBottomRight.x - badgeWidth - 6.0f, cursorY + 6.0f),
			badge,
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::HorizontalRule(rowTopLeft.x, rowBottomRight.x, rowBottomRight.y, tokens::BORDER_SUBTLE);

		if (clicked) {
			state.SetScreen(entry.screen);
		}

		cursorY += ENTRY_HEIGHT;
	}

	DrawFooter_(origin, bottomRight, width, services);

	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(bottomRight.x, origin.y),
		ImVec2(bottomRight.x, bottomRight.y),
		tokens::BORDER
	);
}
}
