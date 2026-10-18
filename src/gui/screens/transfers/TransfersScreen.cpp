#include "gui/screens/transfers/TransfersScreen.h"
#include "gui/screens/transfers/BaselineProgressPanel.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/text/CommonText.h"
#include "gui/text/TransfersText.h"

#include <algorithm>
#include <format>
#include <string>

namespace wgrd::gui {
float TransfersScreen::DrawSwarmCard_(
	const ScreenArea& area,
	const float cursorY,
	domain::ISwarmService* swarm
) const {
	const float cardWidth = area.width * 0.5f;

	design::HeadingBar(ImVec2(area.origin.x, cursorY), cardWidth, text::transfers::SWARM, design::HeadingLevel::Minor);
	design::HeadingBar(
		ImVec2(area.origin.x + cardWidth, cursorY),
		cardWidth,
		text::transfers::LISTENER,
		design::HeadingLevel::Minor
	);

	const float bodyY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);

	if (swarm == nullptr) {
		design::TextAt(ImVec2(area.origin.x + 6.0f, bodyY + 6.0f), text::transfers::OFFLINE, tokens::TEXT_DISABLED, 19.0f);
		return bodyY + 52.0f;
	}

	const domain::SwarmStatus& status = swarm->Status();

	design::TextAt(
		ImVec2(area.origin.x + 6.0f, bodyY + 4.0f),
		std::format(text::transfers::DHT_NODES_FORMAT, status.dhtNodes),
		status.dhtRunning ? tokens::ACCENT : tokens::TEXT_DISABLED,
		19.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 6.0f, bodyY + 30.0f),
		status.dhtRunning ? text::transfers::DHT_RUNNING : text::transfers::DHT_STARTING,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + cardWidth + 6.0f, bodyY + 4.0f),
		std::format(text::transfers::PORT_FORMAT, status.listenPort),
		status.running ? tokens::SUCCESS : tokens::FAILURE,
		19.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + cardWidth + 6.0f, bodyY + 30.0f),
		status.listenInterface,
		tokens::SECONDARY_MUTED,
		10.0f
	);

	const bool heard = status.trackerReplies > 0;

	design::TextAt(
		ImVec2(area.origin.x + 6.0f, bodyY + 42.0f),
		heard
		? std::format(text::transfers::TRACKER_FORMAT, status.trackerReplies, status.trackerPeers)
		: std::string(text::transfers::TRACKER_SILENT),
		heard ? tokens::SUCCESS : tokens::TEXT_DISABLED,
		10.0f
	);

	if (heard) {
		if (status.trackerFailures > 0) {
			design::TextAt(
				ImVec2(area.origin.x + cardWidth + 6.0f, bodyY + 42.0f),
				std::format(text::transfers::TRACKER_UNAVAILABLE_FORMAT, status.trackerFailures),
				tokens::TEXT_DISABLED,
				10.0f
			);
		}
	} else if (!status.lastTrackerError.empty()) {
		design::TextAt(
			ImVec2(area.origin.x + cardWidth + 6.0f, bodyY + 42.0f),
			status.lastTrackerError,
			tokens::WARNING_HEADING,
			10.0f
		);
	}

	return bodyY + 66.0f;
}

float TransfersScreen::DrawGossip_(
	const ScreenArea& area,
	const float cursorY,
	domain::IAnnounceGossip* gossip
) {
	design::HeadingBar(
		ImVec2(area.origin.x, cursorY),
		area.width,
		text::transfers::GOSSIP,
		design::HeadingLevel::Minor,
		gossip != nullptr && gossip->Gossip().running
		? design::HeadingTone::Success
		: design::HeadingTone::Warning
	);

	const float bodyY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);

	if (gossip == nullptr) {
		ScreenToolbar::Placeholder(area, bodyY, text::transfers::GOSSIP_UNAVAILABLE);
		return bodyY + 32.0f;
	}

	const domain::GossipStatus& status = gossip->Gossip();

	design::TextAt(
		ImVec2(area.origin.x + 6.0f, bodyY + 5.0f),
		std::format(text::transfers::PEERS_FORMAT, status.peers),
		status.peers > 0 ? tokens::ACCENT : tokens::SECONDARY_MUTED,
		11.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 110.0f, bodyY + 5.0f),
		std::format(text::transfers::OFFERS_FORMAT, status.offersSent, status.offersReceived),
		tokens::SECONDARY_MUTED,
		10.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 300.0f, bodyY + 5.0f),
		std::format(text::transfers::RECORDS_FORMAT, status.recordsSent, status.recordsReceived),
		tokens::SECONDARY_MUTED,
		10.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 500.0f, bodyY + 5.0f),
		std::format(text::transfers::ACCEPTED_FORMAT, status.recordsAccepted),
		status.recordsAccepted > 0 ? tokens::SUCCESS : tokens::SECONDARY_MUTED,
		10.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 620.0f, bodyY + 5.0f),
		std::format(text::transfers::REJECTED_FORMAT, status.recordsRejected),
		status.recordsRejected > 0 ? tokens::FAILURE : tokens::SECONDARY_MUTED,
		10.0f
	);

	design::TextAt(
		ImVec2(area.origin.x + 740.0f, bodyY + 5.0f),
		std::format(text::transfers::THROTTLED_FORMAT, status.peersThrottled, status.protocolViolations),
		tokens::TEXT_DISABLED,
		10.0f
	);



	if (!status.lastPeerError.empty()) {
		design::TextAt(
			ImVec2(area.origin.x + 6.0f, bodyY + 19.0f),
			std::string(text::transfers::PEER_DROP_PREFIX) + status.lastPeerError,
			tokens::WARNING_HEADING,
			10.0f
		);
	}

	const float bottom = bodyY + 34.0f;
	design::HorizontalRule(area.origin.x, area.origin.x + area.width, bottom, tokens::BORDER_SUBTLE);

	return bottom;
}

float TransfersScreen::DrawSeeding_(
	const ScreenArea& area,
	const float cursorY,
	domain::ISeedingService* seeding
) const {
	if (seeding == nullptr) {
		design::HeadingBar(
			ImVec2(area.origin.x, cursorY),
			area.width,
			text::transfers::SEEDING,
			design::HeadingLevel::Minor
		);

		const float bodyY = cursorY + design::HeadingHeight(design::HeadingLevel::Minor);
		ScreenToolbar::Placeholder(area, bodyY, text::transfers::SEEDING_UNAVAILABLE);
		return bodyY + 32.0f;
	}

	const std::vector<domain::SeedEntry>& entries = seeding->Entries();

	design::HeadingBar(
		ImVec2(area.origin.x, cursorY),
		area.width,
		std::format(
			text::transfers::SEEDING_HEADING_FORMAT,
			entries.size(),
			ScreenToolbar::FormatBytes(seeding->UploadedBytes())
		),
		design::HeadingLevel::Minor,
		seeding->Enabled() ? design::HeadingTone::Success : design::HeadingTone::Warning
	);

	const float headingHeight = design::HeadingHeight(design::HeadingLevel::Minor);

	const std::string_view toggleLabel = seeding->Enabled() ? text::transfers::PAUSE : text::transfers::RESUME;
	const ImVec2 toggleSize = design::ButtonSize(toggleLabel);

	const float toggleTop = cursorY + (headingHeight - toggleSize.y) * 0.5f;

	if (design::Button(
		ImVec2(area.origin.x + area.width - toggleSize.x - 6.0f, toggleTop),
		toggleLabel,
		design::ButtonVariant::Neutral
	)) {
		seeding->SetEnabled(!seeding->Enabled());
	}

	float rowY = std::max(cursorY + headingHeight, toggleTop + toggleSize.y) + 2.0f;

	if (entries.empty()) {
		ScreenToolbar::Placeholder(area, rowY, text::transfers::NOTHING_HELD);
		return rowY + 32.0f;
	}

	for (const domain::SeedEntry& entry : entries) {
		design::Pill(
			ImVec2(area.origin.x + 6.0f, rowY + 4.0f),
			entry.seeding ? text::transfers::PILL_SEEDING : text::transfers::PILL_STARTING,
			entry.seeding ? tokens::SUCCESS : tokens::ADVISORY
		);

		design::TextAt(ImVec2(area.origin.x + 96.0f, rowY + 5.0f), entry.modName, tokens::SECONDARY, 11.0f);

		design::TextAt(
			ImVec2(area.origin.x + 300.0f, rowY + 5.0f),
			std::format(text::common::VERSION_FORMAT, entry.version),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::TextAt(
			ImVec2(area.origin.x + 360.0f, rowY + 5.0f),
			ScreenToolbar::FormatBytes(entry.payloadBytes),
			tokens::SECONDARY,
			10.0f
		);

		design::TextAt(
			ImVec2(area.origin.x + 460.0f, rowY + 5.0f),
			std::format(text::transfers::CHUNKS_FORMAT, entry.chunkCount),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::TextAt(
			ImVec2(area.origin.x + 570.0f, rowY + 5.0f),
			std::format(text::transfers::PEERS_FORMAT, entry.peers),
			entry.peers > 0 ? tokens::ACCENT : tokens::SECONDARY_MUTED,
			10.0f
		);

		design::TextAt(
			ImVec2(area.origin.x + 660.0f, rowY + 5.0f),
			ScreenToolbar::FormatBytes(entry.uploadedBytes),
			tokens::SECONDARY_MUTED,
			10.0f
		);

		design::TextAt(
			ImVec2(area.origin.x + 760.0f, rowY + 5.0f),
			entry.infoHash.substr(0, 16),
			tokens::TEXT_DISABLED,
			9.0f
		);

		rowY += HELD_ROW_HEIGHT;
		design::HorizontalRule(area.origin.x, area.origin.x + area.width, rowY, tokens::BORDER_SUBTLE);
	}

	return rowY;
}

void TransfersScreen::Draw(
	const ScreenArea& area,
	domain::ISwarmService* swarm,
	domain::ISeedingService* seeding,
	domain::IInstallService* install,
	domain::IAnnounceGossip* gossip,
	const domain::IBaselineIndexService* baseline
) {
	const std::uint64_t requested = install != nullptr ? install->RequestedDownloads() : 0;
	float cursorY = ScreenToolbar::Draw(area, text::transfers::TITLE,
		std::format(text::transfers::SESSION_DOWNLOADS, requested));

	cursorY = DrawSwarmCard_(area, cursorY + 8.0f, swarm);
	cursorY = DrawGossip_(area, cursorY + 8.0f, gossip);
	cursorY = DrawSeeding_(area, cursorY + 8.0f, seeding);

	cursorY = BaselineProgressPanel::Draw(area, cursorY + 8.0f, baseline);
	_downloads.Draw(area, cursorY + 8.0f, install);
}
}
