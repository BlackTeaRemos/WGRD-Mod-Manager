#include "gui/screens/catalog/CatalogRowView.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/text/ClippedText.h"
#include "gui/screens/ScreenToolbar.h"
#include "gui/images/PreviewImages.h"

#include <format>

namespace wgrd::gui {
void CatalogRowView::Header(const ScreenArea& area, const float top) {
	design::TextAt(ImVec2(area.origin.x + 56.0f, top), "MOD", tokens::SECONDARY_MUTED, 9.0f);
	if (area.width >= 620.0f) {
		design::TextAt(ImVec2(area.origin.x + area.width - 300.0f, top), "VERSION", tokens::SECONDARY_MUTED, 9.0f);
		design::TextAt(ImVec2(area.origin.x + area.width - 220.0f, top), "SIZE", tokens::SECONDARY_MUTED, 9.0f);
		design::TextAt(ImVec2(area.origin.x + area.width - 145.0f, top), "STATE", tokens::SECONDARY_MUTED, 9.0f);
	}
}

CatalogRowAction CatalogRowView::Draw(const ScreenArea& area, const float top,
	const domain::CatalogRow& row, const bool available, const bool busy) {
	const ImVec2 origin(area.origin.x, top);
	const ImVec2 end(area.origin.x + area.width, top + HEIGHT);
	bool highlighted = false;
	const bool selected = design::RowHit(origin, end, highlighted, true);
	const bool keyboardSelected = selected && ImGui::IsItemFocused() && !ImGui::IsMouseReleased(ImGuiMouseButton_Left);
	const bool keyboardMenu = ImGui::IsItemFocused() && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_F10);
	if (highlighted) {
		design::FillRect(origin, end, tokens::ACCENT_HOVER_FILL);
	}
	design::TextAt(ImVec2(origin.x + 6.0f, top + 10.0f), row.installed ? "*" : "-", row.installed ? tokens::SUCCESS : tokens::TEXT_DISABLED, 11.0f);

	const bool wide = area.width >= 620.0f;
	const bool expanded = area.width >= 1000.0f;
	if (row.preview && !row.revoked && ImGui::IsRectVisible(origin, end)) {
		const bool drawn = PreviewImages::Draw(row.manifestDigest, row.preview->thumbnail,
			ImVec2(origin.x + 22.0f, top + 2.0f), ImVec2(28.0f, 28.0f), true);
		(void)drawn;
	}
	const float nameWidth = area.width - (expanded ? 552.0f : (wide ? 370.0f : 152.0f));
	design::ClippedText::Draw(ImVec2(origin.x + 56.0f, top + 2.0f), nameWidth, row.modName, tokens::SECONDARY, 13.0f, FontRole::Body);
	design::ClippedText::Draw(ImVec2(origin.x + 56.0f, top + 19.0f), nameWidth,
		row.seedFault.empty() ? row.publisher : row.seedFault,
		row.seedFault.empty() ? tokens::SECONDARY_MUTED : tokens::FAILURE, 10.0f);
	if (wide) {
		design::TextAt(ImVec2(end.x - 300.0f, top + 10.0f), std::format("v{}", row.version), tokens::SECONDARY, 11.0f);
		design::TextAt(ImVec2(end.x - 220.0f, top + 10.0f), row.manifestHeld ? ScreenToolbar::FormatBytes(row.totalBytes) : "unknown", tokens::SECONDARY, 10.0f);
		design::ClippedText::Draw(ImVec2(end.x - 145.0f, top + 10.0f), 65.0f,
			row.revoked ? "revoked" : (row.installed ? "installed" : "available"),
			row.revoked ? tokens::FAILURE : (row.installed ? tokens::SUCCESS : tokens::SECONDARY_MUTED), 9.0f);
	}

	if (expanded && row.manifestHeld) {
		design::TextAt(ImVec2(end.x - 490.0f, top + 10.0f), std::format("{} chunks", row.chunkCount), tokens::SECONDARY_MUTED, 10.0f);
		design::TextAt(ImVec2(end.x - 390.0f, top + 10.0f), std::format("{} files", row.fileCount), tokens::SECONDARY_MUTED, 10.0f);
	}

	const bool current = row.VersionKnown() && !row.Outdated();
	const bool actionable = available && !busy && !current && !row.revoked;
	const std::string_view action = !row.installed ? "install" : (row.Outdated() ? "update" : "current");
	const ImVec2 buttonSize = design::ButtonSize(action);
	const ImVec2 button(end.x - buttonSize.x - 8.0f, top + 8.0f);
	const bool install = design::Button(button, action, actionable ? design::ButtonVariant::Accent : design::ButtonVariant::Neutral, actionable, 8.0f, actionable);
	if (!actionable && ImGui::IsWindowHovered() && design::PointerInside(button, ImVec2(button.x + buttonSize.x, button.y + buttonSize.y))) {
		ImGui::SetTooltip("%s", busy ? "Another operation active" : (row.revoked ? "Publisher key revoked" : (current ? "Already current" : "Install unavailable")));
	}
	design::HorizontalRule(origin.x, end.x, end.y, tokens::BORDER_SUBTLE);
	if (install) {
		return CatalogRowAction::Install;
	}
	if (keyboardMenu || design::RowSecondaryHit(origin, end)) {
		return CatalogRowAction::ContextMenu;
	}
	if (keyboardSelected || (selected && !design::PointerInside(button, ImVec2(button.x + buttonSize.x, button.y + buttonSize.y)))) {
		return CatalogRowAction::Details;
	}
	return CatalogRowAction::None;
}
}
