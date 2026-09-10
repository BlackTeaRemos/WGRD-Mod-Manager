#include "gui/screens/transfers/BaselineProgressPanel.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/text/ClippedText.h"
#include "gui/screens/ScreenToolbar.h"

#include <algorithm>
#include <format>

namespace wgrd::gui {
float BaselineProgressPanel::Draw(const ScreenArea& area, float top, const domain::IBaselineIndexService* baseline) {
	if (baseline == nullptr) {
		return top;
	}
	const auto status = baseline->Status();
	design::HeadingBar(ImVec2(area.origin.x, top), area.width, "Game file reuse", design::HeadingLevel::Minor);
	top += design::HeadingHeight(design::HeadingLevel::Minor) + 6.0f;
	design::TextAt(ImVec2(area.origin.x + 6.0f, top), status.running ? "Indexing game files" : "Game index ready",
		status.running ? tokens::ACCENT : tokens::SUCCESS, 11.0f, FontRole::Body);
	top += 17.0f;
	if (!status.activeFile.empty()) {
		design::ClippedText::Draw(ImVec2(area.origin.x + 6.0f, top), area.width - 12.0f, status.activeFile, tokens::SECONDARY, 10.0f);
		top += 16.0f;
	}
	const std::uint64_t processed = status.indexedBytes + status.activeBytes;
	design::TextAt(ImVec2(area.origin.x + 6.0f, top),
		std::format("{} of {}", ScreenToolbar::FormatBytes(processed), ScreenToolbar::FormatBytes(status.totalBytes)), tokens::SECONDARY_MUTED, 10.0f);
	top += 17.0f;
	const float fraction = status.totalBytes == 0 ? 0.0f
		: static_cast<float>(static_cast<double>(processed) / static_cast<double>(status.totalBytes));
	design::Meter(ImVec2(area.origin.x + 6.0f, top), area.width - 12.0f, 9.0f,
		std::clamp(fraction, 0.0f, 1.0f), status.running ? tokens::ACCENT : tokens::SUCCESS);
	top += 15.0f;
	design::TextAt(ImVec2(area.origin.x + 6.0f, top), std::format("{} reusable packs", status.indexedFiles), tokens::SECONDARY_MUTED, 10.0f);
	if (status.failedFiles != 0) {
		design::TextAt(ImVec2(area.origin.x + 200.0f, top), std::format("{} packs unavailable", status.failedFiles), tokens::ADVISORY, 10.0f);
	}
	return top + 22.0f;
}
}
