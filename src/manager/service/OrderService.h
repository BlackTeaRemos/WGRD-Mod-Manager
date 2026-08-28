#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/types/game/GameInstallation.h"
#include "domain/types/order/LoadOrder.h"
#include "manager/inspect/OrderInspector.h"

#include <vector>

namespace wgrd::manager {

class OrderService final : public domain::IOrderService {
public:
    explicit OrderService(domain::GameInstallation installation);
    ~OrderService() override = default;

    [[nodiscard]] const domain::OrderSnapshot& Current() const override;

    void Refresh() override;

    void SetEnabled(const domain::InstallFolder& folder, bool enabled) override;

    void Move(std::size_t fromIndex, std::size_t toIndex) override;

private:
    void Rebuild_();
    void Persist_();
    [[nodiscard]] domain::LoadOrder ComposeOrder_() const;

    domain::GameInstallation _installation;
    std::vector<domain::OrderEntryView> _entries;
    OrderInspector _inspector;
    domain::OrderSnapshot _snapshot;
};

}
