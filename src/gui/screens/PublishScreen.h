#pragma once

#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IPublishService.h"
#include "gui/screens/ScreenArea.h"
#include "gui/state/ApplicationState.h"

#include <array>
#include <cstddef>

namespace wgrd::gui {

class PublishScreen {
public:
    static constexpr std::size_t STEP_COUNT = 3;
    static constexpr std::size_t NAME_CAPACITY = 64;

    void Draw(
        const ScreenArea& area,
        ApplicationState& state,
        domain::IPublishService* publish,
        domain::ICatalogService* catalog);

private:
    [[nodiscard]] float DrawStepBar_(
        const ScreenArea& area,
        float cursorY,
        ApplicationState& state) const;

    [[nodiscard]] float DrawKeyStep_(
        const ScreenArea& area,
        float cursorY,
        domain::IPublishService* publish);

    [[nodiscard]] float DrawSourceStep_(
        const ScreenArea& area,
        float cursorY,
        ApplicationState& state,
        domain::IPublishService* publish) const;

    [[nodiscard]] float DrawSignStep_(
        const ScreenArea& area,
        float cursorY,
        ApplicationState& state,
        domain::IPublishService* publish,
        domain::ICatalogService* catalog) const;

    void DrawHistory_(
        const ScreenArea& area,
        float cursorY,
        domain::IPublishService* publish) const;

    std::array<char, NAME_CAPACITY> _nameBuffer{};
};

}
