#include "gui/screens/transfers/DownloadProgressPanel.h"
#include "gui/design/Tokens.h"
#include "gui/feedback/InstallStartText.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/TransfersText.h"

#include <algorithm>
#include <format>
#include <string>

namespace wgrd::gui {
design::TransferSegments DownloadProgressPanel::SegmentsFor_(const domain::InstallProgress& progress) {
	const std::uint64_t total = progress.heldBytes + progress.remoteBytes;

	if (total == 0) {
		return design::TransferSegments{};
	}

	const auto span = static_cast<double>(total);

	const std::uint64_t verifiedBytes = progress.heldBytes + progress.fetchedBytes;
	const std::uint64_t pendingBytes = progress.pendingWrites * PENDING_WRITE_BLOCK_BYTES;

	const std::uint64_t settledBytes = verifiedBytes > pendingBytes
	                                   ? verifiedBytes - pendingBytes
	                                   : 0;

	const double settled = std::min(static_cast<double>(settledBytes) / span, 1.0);
	const double remaining = 1.0 - settled;

	const double awaiting =
			static_cast<double>(progress.inFlightBytes + pendingBytes) / span;

	const double striped = std::min(awaiting, remaining > 0.0 ? remaining : 0.0);

	return design::TransferSegments{
		static_cast<float>(settled), static_cast<float>(striped)
	};
}

void DownloadProgressPanel::Draw(
	const ScreenArea& area,
	const float cursorY,
	domain::IInstallService* install
) {
	design::HeadingBar(
		ImVec2(area.origin.x, cursorY),
		area.width,
		text::transfers::DOWNLOADS,
		design::HeadingLevel::Minor
	);

	const float bodyY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);

	if (install == nullptr) {
		ScreenToolbar::Placeholder(area, bodyY, text::transfers::INSTALL_UNAVAILABLE);
		return;
	}

	const domain::InstallProgress progress = install->Progress();
	if (progress.Busy()) {
		_retryError.clear();
	}
	if (progress.phase == domain::InstallPhase::Planning) {
		design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 5.0f), progress.modName, tokens::SECONDARY, 12.0f, FontRole::Name);
		design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 22.0f), progress.message, tokens::SECONDARY_MUTED, 10.0f, FontRole::Body);
		design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 39.0f), "Calculating download size", tokens::SECONDARY_MUTED, 10.0f);
		if (design::Button(ImVec2(area.origin.x + area.width - 70.0f, bodyY + 4.0f), "Cancel", design::ButtonVariant::Failure)) {
			install->Cancel();
		}
		return;
	}

	if (progress.phase == domain::InstallPhase::Idle) {
		ScreenToolbar::Placeholder(area, bodyY, text::transfers::IDLE);
		return;
	}

	design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 5.0f), progress.modName, tokens::SECONDARY, 12.0f, FontRole::Name);

	const ImU32 tone =
			progress.phase == domain::InstallPhase::Failed ? tokens::FAILURE : (progress.phase == domain::InstallPhase::Done ? tokens::SUCCESS : tokens::ACCENT);

	design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 20.0f), _retryError.empty() ? progress.message : _retryError, tone, 10.0f, FontRole::Body);

	design::TextAt(
		ImVec2(area.origin.x + 300.0f, bodyY + 5.0f),
		std::format(text::transfers::TO_FETCH_FORMAT, ScreenToolbar::FormatBytes(progress.remoteBytes)),
		tokens::SECONDARY,
		10.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 300.0f, bodyY + 20.0f),
		std::format(text::transfers::REUSED_FORMAT, ScreenToolbar::FormatBytes(progress.heldBytes)),
		tokens::SUCCESS,
		10.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 460.0f, bodyY + 5.0f),
		std::format(text::transfers::CHUNKS_FORMAT, progress.remoteChunks),
		tokens::SECONDARY_MUTED,
		10.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 460.0f, bodyY + 20.0f),
		std::format(text::transfers::PEERS_FORMAT, progress.peers),
		tokens::SECONDARY_MUTED,
		10.0f
	);

	const std::uint64_t total = progress.heldBytes + progress.remoteBytes;
	const std::uint64_t moved = progress.heldBytes + progress.fetchedBytes;

	if (progress.Busy()) {
		if (design::Button(
			ImVec2(area.origin.x + area.width - 70.0f, bodyY + 4.0f),
			text::transfers::CANCEL,
			design::ButtonVariant::Failure
		)) {
			install->Cancel();
		}
	}

	const float barY = bodyY + DOWNLOAD_BAR_TOP_OFFSET;
	const float barWidth = area.width - DOWNLOAD_BAR_INSET * 2.0f;

	design::TransferBar(
		ImVec2(area.origin.x + DOWNLOAD_BAR_INSET, barY),
		barWidth,
		DOWNLOAD_BAR_HEIGHT,
		SegmentsFor_(progress)
	);

	const float captionY = barY + DOWNLOAD_BAR_HEIGHT + 4.0f;

	design::TextAt(
		ImVec2(area.origin.x + DOWNLOAD_BAR_INSET, captionY),
		total == 0
		? std::string(text::transfers::NOTHING_TO_MOVE)
		: std::format(
			text::transfers::MOVED_FORMAT,
			ScreenToolbar::FormatBytes(moved),
			ScreenToolbar::FormatBytes(total)
		),
		tokens::SECONDARY_MUTED,
		10.0f
	);

	if (progress.phase == domain::InstallPhase::Failed) {
		const float buttons = captionY + 20.0f;
		if (design::Button(ImVec2(area.origin.x + 6.0f, buttons), "Retry", design::ButtonVariant::Accent)) {
			const auto retried = progress.verification ? install->Verify(progress.identifier) : install->Start(progress.identifier);
			_retryError = retried.has_value() ? "" : std::string(InstallStartText::Describe(retried.error()));
		}
		if (design::Button(ImVec2(area.origin.x + 80.0f, buttons), "Copy error", design::ButtonVariant::Neutral)) {
			ImGui::SetClipboardText((_retryError.empty() ? progress.message : _retryError).c_str());
		}
	}

	if (total > 0) {
		const double share = std::min(static_cast<double>(moved) / static_cast<double>(total), 1.0);
		const std::string percent = std::format(text::transfers::SHARE_FORMAT, share * 100.0);

		design::TextAt(
			ImVec2(
				area.origin.x + area.width - DOWNLOAD_BAR_INSET - design::MeasureText(percent, 10.0f).x,
				captionY
			),
			percent,
			progress.settling ? tokens::ACCENT : tokens::SECONDARY,
			10.0f
		);
	}
}

}
