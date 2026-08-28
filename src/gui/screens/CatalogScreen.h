#pragma once

#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IInstallService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

namespace wgrd::gui {

class CatalogScreen {
public:
    static constexpr float ROW_HEIGHT = 30.0f;

    void Draw(
        const ScreenArea& area,
        ApplicationState& state,
        domain::ICatalogService* catalog,
        domain::IInstallService* install);

private:
    static bool Matches_(const domain::CatalogRow& row, std::size_t filter);

    void DrawRow_(
        const ScreenArea& area,
        float cursorY,
        const domain::CatalogRow& row,
        ApplicationState& state,
        domain::IInstallService* install) const;
};

}
