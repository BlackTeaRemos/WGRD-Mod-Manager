#include "gui/screens/order/OrderDragController.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cstring>
#include <string_view>

namespace wgrd::gui {
void OrderDragController::Begin() {
	_source.clear();
	_insertion.reset();
	const ImGuiPayload* payload = ImGui::GetDragDropPayload();
	if (payload != nullptr && payload->IsDataType("WGRD_ORDER")) {
		const ImGuiWindow* window = ImGui::GetCurrentWindow();
		const ImVec2 pointer = ImGui::GetIO().MousePos;
		if (pointer.x >= window->ClipRect.Min.x && pointer.x <= window->ClipRect.Max.x) {
			const float movement = ImGui::GetIO().DeltaTime * 240.0f;
			if (pointer.y < window->ClipRect.Min.y + 22.0f) {
				ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - movement));
			} else if (pointer.y > window->ClipRect.Max.y - 22.0f) {
				ImGui::SetScrollY(ImGui::GetScrollY() + movement);
			}
		}
	}
}

std::optional<float> OrderDragController::Row(const ImVec2 topLeft, const ImVec2 bottomRight,
	const domain::InstallFolder& folder, const std::size_t position) {
	if (ImGui::BeginDragDropSource()) {
		ImGui::SetDragDropPayload("WGRD_ORDER", folder.Value().c_str(), folder.Value().size() + 1, ImGuiCond_Once);
		ImGui::TextUnformatted(folder.Value().c_str());
		ImGui::EndDragDropSource();
	}
	std::optional<float> marker;
	if (ImGui::BeginDragDropTarget()) {
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WGRD_ORDER",
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
		if (payload != nullptr && payload->DataSize > 1 && payload->DataSize <= 256) {
			const auto* source = static_cast<const char*>(payload->Data);
			const bool after = ImGui::GetIO().MousePos.y >= (topLeft.y + bottomRight.y) * 0.5f;
			marker = after ? bottomRight.y : topLeft.y;
			if (payload->IsDelivery() && source[payload->DataSize - 1] == '\0') {
				_source.assign(source, static_cast<std::size_t>(payload->DataSize - 1));
				_insertion = position + (after ? 1 : 0);
			}
		}
		ImGui::EndDragDropTarget();
	}
	return marker;
}

bool OrderDragController::Commit(domain::IOrderService& order) {
	if (!_insertion.has_value()) {
		return false;
	}
	const auto& entries = order.Current().entries;
	const auto found = std::ranges::find_if(entries, [this](const domain::OrderEntryView& entry) {
		return entry.folder.Value() == _source;
	});
	if (found == entries.end()) {
		return false;
	}
	const std::size_t source = static_cast<std::size_t>(found - entries.begin());
	const std::size_t boundary = std::min(*_insertion, entries.size());
	const std::size_t target = boundary > source ? boundary - 1 : boundary;
	if (target == source) {
		return false;
	}
	order.Move(source, target);
	return true;
}
}
