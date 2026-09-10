#pragma once

#include "domain/interfaces/order/IOrderService.h"
#include "domain/interfaces/order/IOrderHistory.h"
#include "domain/interfaces/order/IOrderIdentity.h"
#include "domain/interfaces/preferences/IWindowPreferences.h"
#include "domain/interfaces/order/IProfileService.h"
#include "domain/interfaces/platform/IFilePicker.h"
#include "domain/interfaces/platform/IPathActions.h"
#include "domain/interfaces/platform/IGameLocation.h"
#include "domain/interfaces/platform/IUriLauncher.h"
#include "domain/interfaces/services/IAnnounceGossip.h"
#include "domain/interfaces/services/ICatalogService.h"
#include "domain/interfaces/services/IInstallService.h"
#include "domain/interfaces/services/IBaselineIndexService.h"
#include "domain/interfaces/services/IModRemovalService.h"
#include "domain/interfaces/services/IMirrorService.h"
#include "domain/interfaces/services/IPatcherService.h"
#include "domain/interfaces/services/IPublishService.h"
#include "domain/interfaces/services/ISeedingService.h"
#include "domain/interfaces/services/ISwarmService.h"
#include "domain/interfaces/services/IUpdateService.h"
#include "domain/interfaces/trust/IRegistryUpdater.h"

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
	const domain::IFilePicker* files = nullptr;
	const domain::IUriLauncher* uris = nullptr;
	domain::IAnnounceGossip* gossip = nullptr;
	domain::IProfileService* profiles = nullptr;
	domain::IModRemovalService* removal = nullptr;
	domain::IPatcherService* patcher = nullptr;
	domain::IMirrorService* mirror = nullptr;
	const domain::IWindowPreferences* windowPreferences = nullptr;
	domain::IOrderHistory* orderHistory = nullptr;
	const domain::IBaselineIndexService* baseline = nullptr;
	const domain::IPathActions* paths = nullptr;
	domain::IGameLocation* gameLocation = nullptr;
	const domain::IOrderIdentity* orderIdentity = nullptr;
};
}
