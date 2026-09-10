#include "gui/screens/order/OrderEditToolbar.h"

#include "gui/design/Primitives.h"
#include "gui/design/Tokens.h"
#include "gui/design/text/ClippedText.h"

namespace wgrd::gui {
float OrderEditToolbar::Draw(const ScreenArea& area, const float top, const ApplicationServices& services) {
	const auto& snapshot = services.order->Current();
	const bool available = services.orderHistory != nullptr && services.orderHistory->CanUndo();
	float left = area.origin.x + 6.0f;
	const bool undo = design::Button(ImVec2(left, top + 4.0f), "Undo",
		design::ButtonVariant::Neutral, false, 8.0f, available);
	const bool shortcut = !ImGui::GetIO().WantTextInput && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z);
	if ((undo || shortcut) && available) {
		const bool applied = services.orderHistory->Undo();
		(void)applied;
		_notice.clear();
	}
	left += design::ButtonSize("Undo").x + 6.0f;
	if (design::Button(ImVec2(left, top + 4.0f), "Copy order hash", design::ButtonVariant::Neutral,
		false, 8.0f, services.orderIdentity != nullptr)) {
		const auto hash = services.orderIdentity->ReadHash();
		_noticeError = !hash.has_value();
		if (hash.has_value()) {
			ImGui::SetClipboardText(hash->ToHex().c_str());
			_notice = "Order hash copied";
		} else {
			_notice = hash.error() == domain::OrderIdentityError::TooLarge ? "Order file too large" : "Order file unavailable";
		}
	}
	left += design::ButtonSize("Copy order hash").x + 6.0f;
	if (design::Button(ImVec2(left, top + 4.0f), "Copy order path", design::ButtonVariant::Neutral)) {
		ImGui::SetClipboardText(snapshot.orderFile.c_str());
		_notice = "Order path copied";
		_noticeError = false;
	}
	left += design::ButtonSize("Copy order path").x + 6.0f;
	if (services.paths != nullptr) {
		if (design::Button(ImVec2(left, top + 4.0f), "Open mods", design::ButtonVariant::Neutral)) {
			const auto path = std::filesystem::path(snapshot.gameRoot) / snapshot.modsDirectory;
			_noticeError = !services.paths->OpenFolder(path).has_value();
			_notice = _noticeError ? "Folder could not open" : "";
		}
	}
	float bottom = top + 28.0f;
	if (services.orderHistory != nullptr) {
		design::ClippedText::Draw(ImVec2(area.origin.x + 6.0f, bottom), area.width - 120.0f,
			services.orderHistory->LastMessage(), snapshot.writable ? tokens::SECONDARY_MUTED : tokens::FAILURE,
			10.0f, FontRole::Body);
		if (!snapshot.writable && design::Button(ImVec2(area.origin.x + area.width - 100.0f, bottom),
			"Copy error", design::ButtonVariant::Neutral)) {
			ImGui::SetClipboardText(std::string(services.orderHistory->LastMessage()).c_str());
		}
		bottom += 23.0f;
	}
	if (!_notice.empty()) {
		design::ClippedText::Draw(ImVec2(area.origin.x + 6.0f, bottom), area.width - 12.0f, _notice,
			_noticeError ? tokens::FAILURE : tokens::SUCCESS, 10.0f, FontRole::Body);
		bottom += 18.0f;
	}
	return bottom;
}
}
