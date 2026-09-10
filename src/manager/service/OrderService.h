#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/order/IOrderHistory.h"
#include "domain/types/game/GameInstallation.h"
#include "domain/types/order/LoadOrder.h"
#include "manager/inspect/OrderInspector.h"

#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace wgrd::manager {
class OrderService final : public domain::IOrderService, public domain::IOrderHistory {
public:
	explicit OrderService(domain::GameInstallation installation);
	~OrderService() override = default;
	[[nodiscard]] const domain::OrderSnapshot& Current() const override;
	void Refresh() override;
	void SetEnabled(const domain::InstallFolder& folder, bool enabled) override;
	void Move(std::size_t fromIndex, std::size_t toIndex) override;
	bool Apply(const domain::LoadOrder& order) override;
	void SetChangeHandler(std::function<void()> handler);
	[[nodiscard]] bool CanUndo() const override;
	[[nodiscard]] bool Undo() override;
	[[nodiscard]] std::string_view LastMessage() const override;

private:
	static constexpr std::size_t HISTORY_LIMIT = 64;
	void Announce_() const;
	void Rebuild_();
	[[nodiscard]] bool Commit_(std::vector<domain::OrderEntryView> entries, bool remember, bool announce);
	[[nodiscard]] static domain::LoadOrder ComposeOrder_(const std::vector<domain::OrderEntryView>& entries);
	[[nodiscard]] std::vector<domain::OrderEntryView> Expand_(const domain::LoadOrder& order) const;

	domain::GameInstallation _installation;
	std::vector<domain::OrderEntryView> _entries;
	OrderInspector _inspector;
	domain::OrderSnapshot _snapshot;
	std::function<void()> _onChanged;
	std::deque<domain::LoadOrder> _undo;
	std::string _message;
};
}
