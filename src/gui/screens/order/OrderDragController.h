#pragma once

#include "domain/interfaces/order/IOrderService.h"

#include <imgui.h>
#include <optional>
#include <string>

namespace wgrd::gui {
class OrderDragController {
public:
	void Begin();
	[[nodiscard]] std::optional<float> Row(ImVec2 topLeft, ImVec2 bottomRight,
		const domain::InstallFolder& folder, std::size_t position);
	[[nodiscard]] bool Commit(domain::IOrderService& order);

private:
	std::string _source;
	std::optional<std::size_t> _insertion;
};
}
