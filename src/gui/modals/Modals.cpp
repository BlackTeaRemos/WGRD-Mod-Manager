#include "gui/modals/Modals.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/ModalText.h"

#include <format>
#include <string>


namespace wgrd::gui {
namespace {
	constexpr float SETTINGS_WIDTH = 520.0f;
	constexpr float DETAIL_WIDTH = 660.0f;
	constexpr float MODAL_TITLE_HEIGHT = 22.0f;
	constexpr float SETTINGS_HEIGHT = 452.0f;
	constexpr float DETAIL_HEIGHT = 300.0f;

	bool ModalTitle(const ImVec2 origin, const float width, const std::string_view title, const std::string_view meta) {
		const ImVec2 bottomRight(origin.x + width, origin.y + MODAL_TITLE_HEIGHT);

		design::FillRect(origin, bottomRight, tokens::ACCENT);
		design::TextAt(ImVec2(origin.x + 6.0f, origin.y + 4.0f), title, tokens::PRIMARY, 13.0f);

		if (!meta.empty()) {
			const float titleWidth = design::MeasureText(title, 13.0f).x;
			design::TextAt(
				ImVec2(origin.x + 14.0f + titleWidth, origin.y + 6.0f),
				meta,
				tokens::FromHex(0x0E0E12, 0.75f),
				10.0f
			);
		}

		const ImVec2 closeTopLeft(bottomRight.x - MODAL_TITLE_HEIGHT, origin.y);
		bool hovered = false;
		const bool clicked = design::RowHit(closeTopLeft, bottomRight, hovered);

		if (hovered) {
			design::FillRect(closeTopLeft, bottomRight, tokens::FromHex(0x0E0E12, 0.12f));
		}

		const ImVec2 extent = design::MeasureText(text::modal::CLOSE_GLYPH, 11.0f);
		design::TextAt(
			ImVec2(
				closeTopLeft.x + (MODAL_TITLE_HEIGHT - extent.x) * 0.5f,
				origin.y + (MODAL_TITLE_HEIGHT - extent.y) * 0.5f
			),
			text::modal::CLOSE_GLYPH,
			tokens::PRIMARY,
			11.0f
		);

		return clicked;
	}

	void ModalShell(const ImVec2 origin, const float width, const float height) {
		design::FillRect(
			ImVec2(origin.x + tokens::SHADOW_OFFSET, origin.y + tokens::SHADOW_OFFSET),
			ImVec2(origin.x + width + tokens::SHADOW_OFFSET, origin.y + height + tokens::SHADOW_OFFSET),
			tokens::FromHex(0x000000, 0.9f)
		);

		design::FillRect(origin, ImVec2(origin.x + width, origin.y + height), tokens::PRIMARY);
		design::StrokeRect(origin, ImVec2(origin.x + width, origin.y + height), tokens::ACCENT);
	}

	bool ToggleRow(const ImVec2 origin, const float width, const std::string_view label, const std::string_view hint, const bool value) {
		design::TextAt(ImVec2(origin.x + 6.0f, origin.y + 2.0f), label, tokens::SECONDARY, 11.0f);
		design::TextAt(ImVec2(origin.x + 6.0f, origin.y + 15.0f), hint, tokens::SECONDARY_MUTED, 10.0f);

		const std::string_view text = value ? text::common::ON : text::common::OFF;
		const ImVec2 size = design::ButtonSize(text);
		const ImVec2 topLeft(origin.x + width - size.x - 6.0f, origin.y + 4.0f);

		return design::Button(
			topLeft,
			text,
			value ? design::ButtonVariant::Success : design::ButtonVariant::Neutral
		);
	}
}

void Modals::Reserve(const ImVec2 frameOrigin, const float frameWidth, const ApplicationState& state) const {
	if (state.SettingsOpen()) {
		const ImVec2 origin(frameOrigin.x + (frameWidth - SETTINGS_WIDTH) * 0.5f, frameOrigin.y + 70.0f);
		design::ReserveRegion(origin, ImVec2(origin.x + SETTINGS_WIDTH, origin.y + SETTINGS_HEIGHT));
		return;
	}

	if (state.DetailOpen()) {
		const ImVec2 origin(frameOrigin.x + (frameWidth - DETAIL_WIDTH) * 0.5f, frameOrigin.y + 104.0f);
		design::ReserveRegion(origin, ImVec2(origin.x + DETAIL_WIDTH, origin.y + DETAIL_HEIGHT));
		return;
	}

	design::ClearReservedRegion();
}

void Modals::Draw(const ImVec2 frameOrigin, const float frameWidth, ApplicationState& state, const ApplicationServices& services) {
	design::ClearReservedRegion();

	if (state.DetailOpen()) {
		if (state.SettingsOpen()) {
			const ImVec2 origin(frameOrigin.x + (frameWidth - SETTINGS_WIDTH) * 0.5f, frameOrigin.y + 70.0f);
			design::ReserveRegion(origin, ImVec2(origin.x + SETTINGS_WIDTH, origin.y + SETTINGS_HEIGHT));
		}
		DrawDetail_(frameOrigin, frameWidth, state, services);
		design::ClearReservedRegion();
	}

	if (state.SettingsOpen()) {
		DrawSettings_(frameOrigin, frameWidth, state, services);
	}

	design::ClearReservedRegion();
}

void Modals::DrawSettings_(const ImVec2 frameOrigin, const float frameWidth, ApplicationState& state, const ApplicationServices& services) {
	constexpr float height = SETTINGS_HEIGHT;
	const ImVec2 origin(frameOrigin.x + (frameWidth - SETTINGS_WIDTH) * 0.5f, frameOrigin.y + 70.0f);

	ModalShell(origin, SETTINGS_WIDTH, height);

	if (ModalTitle(origin, SETTINGS_WIDTH, text::modal::SETTINGS, "")) {
		state.CloseSettings();
	}

	float cursorY = origin.y + MODAL_TITLE_HEIGHT;

	design::HeadingBar(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, text::modal::GAME_LOCATION, design::HeadingLevel::Minor);
	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	design::FillRect(
		ImVec2(origin.x + 6.0f, cursorY + 6.0f),
		ImVec2(origin.x + SETTINGS_WIDTH - 70.0f, cursorY + 28.0f),
		tokens::SURFACE_SUNKEN
	);
	design::StrokeRect(
		ImVec2(origin.x + 6.0f, cursorY + 6.0f),
		ImVec2(origin.x + SETTINGS_WIDTH - 70.0f, cursorY + 28.0f),
		tokens::BORDER
	);
	const bool located = services.order != nullptr && services.order->Current().located;

	const std::string gameRoot = located
	                             ? services.order->Current().gameRoot
	                             : std::string(text::modal::NO_INSTALLATION);

	design::TextAt(
		ImVec2(origin.x + 12.0f, cursorY + 12.0f),
		gameRoot,
		located ? tokens::SECONDARY : tokens::TEXT_DISABLED,
		10.0f
	);
	cursorY += 34.0f;

	if (located) {
		design::TextAt(
			ImVec2(origin.x + 6.0f, cursorY),
			std::format(text::modal::INSTALLED_FORMAT, services.order->Current().installed.size()),
			tokens::SUCCESS,
			10.0f
		);
	} else {
		design::TextAt(ImVec2(origin.x + 6.0f, cursorY), text::modal::PLACE_BESIDE_GAME, tokens::ADVISORY, 10.0f);
	}
	cursorY += 18.0f;

	design::HeadingBar(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, text::modal::TRANSFER, design::HeadingLevel::Minor);
	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	const bool seeding = services.seeding != nullptr && services.seeding->Enabled();

	if (ToggleRow(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, text::modal::SEEDING, text::modal::SEEDING_HINT, seeding)
	    && services.seeding != nullptr) {
		services.seeding->SetEnabled(!seeding);
	}
	cursorY += 30.0f;

	design::TextAt(ImVec2(origin.x + 6.0f, cursorY), text::modal::TRANSPORT_NOTE, tokens::TEXT_DISABLED, 10.0f);
	cursorY += 14.0f;
	design::TextAt(ImVec2(origin.x + 6.0f, cursorY), text::modal::CHUNKING_NOTE, tokens::TEXT_DISABLED, 10.0f);
	cursorY += 22.0f;

	cursorY = DrawTrust_(origin, cursorY, services.registry);

	cursorY = DrawUpdates_(origin, cursorY, state, services.updates);

	if (design::Button(
		ImVec2(origin.x + SETTINGS_WIDTH - 60.0f, origin.y + height - 26.0f),
		text::modal::DONE,
		design::ButtonVariant::Accent,
		true,
		12.0f
	)) {
		state.CloseSettings();
	}
}

const domain::CatalogRow* Modals::FindRow_(
	const std::string_view identifier,
	const ApplicationServices& services
) {
	if (services.catalog == nullptr) {
		return nullptr;
	}

	for (const domain::CatalogRow& row : services.catalog->Rows()) {
		if (row.identifier == identifier) {
			return &row;
		}
	}

	return nullptr;
}

void Modals::DrawDetail_(
	const ImVec2 frameOrigin,
	const float frameWidth,
	ApplicationState& state,
	const ApplicationServices& services
) {
	constexpr float height = DETAIL_HEIGHT;
	const ImVec2 origin(frameOrigin.x + (frameWidth - DETAIL_WIDTH) * 0.5f, frameOrigin.y + 104.0f);

	ModalShell(origin, DETAIL_WIDTH, height);

	const domain::CatalogRow* const row = FindRow_(state.DetailTarget(), services);

	const std::string meta = row == nullptr
	                         ? std::string(text::modal::DETAIL_GONE)
	                         : std::format(
		                         text::modal::DETAIL_META_FORMAT,
		                         row->version,
		                         ScreenToolbar::FormatBytes(row->totalBytes),
		                         row->chunkCount,
		                         row->fileCount
	                         );

	if (ModalTitle(origin, DETAIL_WIDTH, row == nullptr ? state.DetailTarget() : row->modName, meta)) {
		state.CloseDetail();
	}

	float cursorY = origin.y + MODAL_TITLE_HEIGHT + 8.0f;

	if (row == nullptr) {
		design::TextAt(
			ImVec2(origin.x + 8.0f, cursorY),
			text::modal::DETAIL_GONE_BODY,
			tokens::TEXT_DISABLED,
			12.0f
		);
		return;
	}

	design::TextAt(
		ImVec2(origin.x + 8.0f, cursorY),
		text::modal::DETAIL_BODY_FIRST,
		tokens::SECONDARY,
		12.0f
	);
	cursorY += 16.0f;
	design::TextAt(
		ImVec2(origin.x + 8.0f, cursorY),
		text::modal::DETAIL_BODY_SECOND,
		tokens::SECONDARY,
		12.0f
	);
	cursorY += 24.0f;

	design::HeadingBar(ImVec2(origin.x, cursorY), DETAIL_WIDTH, text::modal::IDENTITY, design::HeadingLevel::Minor);
	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	design::TextAt(ImVec2(origin.x + 8.0f, cursorY + 4.0f), text::modal::FIELD_MOD, tokens::SECONDARY_MUTED, 10.0f);
	design::TextAt(ImVec2(origin.x + 90.0f, cursorY + 4.0f), row->modName, tokens::SECONDARY, 11.0f);
	cursorY += 18.0f;

	design::TextAt(ImVec2(origin.x + 8.0f, cursorY + 4.0f), text::modal::FIELD_PUBLISHER, tokens::SECONDARY_MUTED, 10.0f);
	design::TextAt(ImVec2(origin.x + 90.0f, cursorY + 4.0f), row->publisher, tokens::ACCENT, 11.0f);
	cursorY += 18.0f;

	design::TextAt(ImVec2(origin.x + 8.0f, cursorY + 4.0f), text::modal::FIELD_INSTALLED, tokens::SECONDARY_MUTED, 10.0f);
	design::TextAt(
		ImVec2(origin.x + 90.0f, cursorY + 4.0f),
		row->installed ? text::common::YES : text::common::NO,
		row->installed ? tokens::SUCCESS : tokens::SECONDARY_MUTED,
		11.0f
	);
	cursorY += 26.0f;

	design::HeadingBar(
		ImVec2(origin.x, cursorY),
		DETAIL_WIDTH,
		text::modal::SIGNATURE,
		design::HeadingLevel::Minor,
		row->manifestHeld ? design::HeadingTone::Success : design::HeadingTone::Warning
	);
	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	design::TextAt(
		ImVec2(origin.x + 8.0f, cursorY + 4.0f),
		row->manifestHeld ? text::modal::MANIFEST_VERIFIED : text::modal::MANIFEST_ABSENT,
		row->manifestHeld ? tokens::SUCCESS : tokens::ADVISORY,
		10.0f
	);

	design::TextAt(
		ImVec2(origin.x + 8.0f, cursorY + 18.0f),
		row->identifier,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	if (services.install == nullptr) {
		return;
	}

	const bool busy = services.install->Progress().Busy();

	if (design::Button(
		    ImVec2(origin.x + 8.0f, origin.y + height - 28.0f),
		    row->installed ? text::modal::ACTION_REINSTALL : text::modal::ACTION_INSTALL,
		    design::ButtonVariant::Accent,
		    !busy,
		    12.0f
	    )
	    && !busy) {
		const auto started = services.install->Start(row->identifier);
		if (started.has_value()) {
			state.CloseDetail();
			state.SetScreen(Screen::Transfers);
		}
	}
}


float Modals::DrawUpdates_(const ImVec2 origin, float cursorY, ApplicationState& state, domain::IUpdateService* updates) const {
	design::HeadingBar(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, text::modal::UPDATES, design::HeadingLevel::Minor);
	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	if (updates == nullptr) {
		design::TextAt(
			ImVec2(origin.x + 6.0f, cursorY + 6.0f),
			text::modal::UPDATES_UNAVAILABLE,
			tokens::TEXT_DISABLED,
			10.0f
		);
		return cursorY + 30.0f;
	}

	const domain::UpdateStatus status = updates->Status();

	const std::string versions = status.latestVersion.empty()
	                             ? std::format(text::modal::INSTALLED_VERSION_FORMAT, status.currentVersion)
	                             : std::format(text::modal::VERSION_PAIR_FORMAT, status.currentVersion, status.latestVersion);

	design::TextAt(ImVec2(origin.x + 6.0f, cursorY + 6.0f), versions, tokens::SECONDARY, 10.0f);

	const ImU32 messageColor =
			status.phase == domain::UpdatePhase::Failed
			? tokens::FAILURE
			: (status.phase == domain::UpdatePhase::Ready ? tokens::SUCCESS : (status.phase == domain::UpdatePhase::Available ? tokens::ACCENT : tokens::SECONDARY_MUTED));

	design::TextAt(ImVec2(origin.x + 6.0f, cursorY + 20.0f), status.message, messageColor, 10.0f);

	if (status.phase == domain::UpdatePhase::Downloading) {
		const float fraction = status.totalBytes == 0
		                       ? 0.0f
		                       : static_cast<float>(
			                       static_cast<double>(status.downloadedBytes) / static_cast<double>(status.totalBytes));

		design::Meter(
			ImVec2(origin.x + SETTINGS_WIDTH - 200.0f, cursorY + 12.0f),
			190.0f,
			9.0f,
			fraction,
			tokens::ACCENT
		);

		design::TextAt(
			ImVec2(origin.x + SETTINGS_WIDTH - 200.0f, cursorY + 24.0f),
			std::format(
				text::modal::DOWNLOAD_PROGRESS_FORMAT,
				static_cast<double>(status.downloadedBytes) / 1048576.0,
				static_cast<double>(status.totalBytes) / 1048576.0
			),
			tokens::SECONDARY_MUTED,
			9.0f
		);

		return cursorY + 44.0f;
	}

	if (status.Busy()) {
		return cursorY + 40.0f;
	}

	if (status.phase == domain::UpdatePhase::Available) {
		if (design::Button(
			ImVec2(origin.x + SETTINGS_WIDTH - 180.0f, cursorY + 8.0f),
			text::modal::DOWNLOAD_UPDATE,
			design::ButtonVariant::Accent,
			true
		)) {
			updates->Download();
		}
		return cursorY + 40.0f;
	}

	if (status.phase == domain::UpdatePhase::Ready) {
		if (design::Button(
			ImVec2(origin.x + SETTINGS_WIDTH - 190.0f, cursorY + 8.0f),
			text::modal::RESTART_UPDATE,
			design::ButtonVariant::Success,
			true
		)) {
			if (updates->ApplyAndRestart()) {
				state.RequestExit();
			}
		}
		return cursorY + 40.0f;
	}

	if (design::Button(
		ImVec2(origin.x + SETTINGS_WIDTH - 170.0f, cursorY + 8.0f),
		text::modal::CHECK_UPDATES,
		design::ButtonVariant::Neutral
	)) {
		updates->Check();
	}

	return cursorY + 40.0f;
}


float Modals::DrawTrust_(const ImVec2 origin, float cursorY, domain::IRegistryUpdater* updater) const {
	design::HeadingBar(ImVec2(origin.x, cursorY), SETTINGS_WIDTH, text::modal::TRUST_REGISTRY, design::HeadingLevel::Minor);
	cursorY += design::HeadingHeight(design::HeadingLevel::Minor);

	if (updater == nullptr) {
		design::TextAt(
			ImVec2(origin.x + 6.0f, cursorY + 6.0f),
			text::modal::REGISTRY_UNAVAILABLE,
			tokens::TEXT_DISABLED,
			10.0f
		);
		return cursorY + 30.0f;
	}

	const domain::RegistryStatus status = updater->Status();

	design::TextAt(
		ImVec2(origin.x + 6.0f, cursorY + 6.0f),
		std::format(text::modal::KEYS_TRUSTED_FORMAT, status.keyCount),
		tokens::SECONDARY,
		10.0f
	);

	const ImU32 messageColor =
			status.phase == domain::RegistryPhase::Failed ? tokens::FAILURE : (status.phase == domain::RegistryPhase::Fresh ? tokens::SUCCESS : tokens::ADVISORY);

	const std::string detail = status.phase == domain::RegistryPhase::Fresh
	                           ? std::format(text::modal::SYNCED_FORMAT, status.message, status.secondsSincePoll)
	                           : status.message;

	design::TextAt(ImVec2(origin.x + 6.0f, cursorY + 20.0f), detail, messageColor, 10.0f);

	if (!status.Busy()) {
		if (design::Button(
			ImVec2(origin.x + SETTINGS_WIDTH - 150.0f, cursorY + 8.0f),
			text::modal::SYNC_REGISTRY,
			design::ButtonVariant::Neutral
		)) {
			updater->Poll();
		}
	}

	return cursorY + 40.0f;
}
}
