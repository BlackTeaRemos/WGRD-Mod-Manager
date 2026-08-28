#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "gui/screens/ScreenArea.h"

namespace wgrd::gui {

class OrderScreen {
public:
    static constexpr float ROW_HEIGHT = 34.0f;

    void Draw(const ScreenArea& area, domain::IOrderService* orderService);

private:
    [[nodiscard]] bool DrawEntries_(
        const ScreenArea& area,
        float cursorY,
        float rightX,
        const domain::OrderSnapshot& snapshot,
        domain::IOrderService* orderService) const;

    void DrawSidebar_(
        const ScreenArea& area,
        float cursorY,
        float rightX,
        const domain::OrderSnapshot& snapshot) const;
};

}
