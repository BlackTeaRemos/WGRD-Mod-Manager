#pragma once

#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/services/IPublishService.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/services/ISwarmService.h"
#include "domain/interfaces/trust/IRegistryUpdater.h"
#include "domain/interfaces/services/IUpdateService.h"

namespace wgrd::gui {

struct ApplicationServices {
    domain::IOrderService* order = nullptr;
    domain::ICatalogService* catalog = nullptr;
    domain::IPublishService* publish = nullptr;
    domain::ISwarmService* swarm = nullptr;
    domain::ISeedingService* seeding = nullptr;
    domain::IInstallService* install = nullptr;
    domain::IUpdateService* updates = nullptr;
    domain::IRegistryUpdater* registry = nullptr;
};

}
