#pragma once

#include "domain/types/order/Annotation.h"
#include "domain/types/order/InstalledMod.h"
#include "domain/types/order/InstallFolder.h"
#include "domain/types/order/LoadOrder.h"

#include <string>
#include <vector>

namespace wgrd::domain {
struct OrderEntryView {
	InstallFolder folder;
	bool enabled;
	bool present;
};

struct OrderSnapshot {
	std::vector<OrderEntryView> entries;
	std::vector<Annotation> annotations;
	std::vector<InstalledMod> installed;
	std::string filePreview;
	std::string gameRoot;
	std::string modsDirectory;
	std::string orderFile;
	std::size_t enabledCount = 0;
	bool located = false;
	bool writable = false;
};

class IOrderService {
public:
	virtual ~IOrderService() = 0;

	[[nodiscard]] virtual const OrderSnapshot& Current() const = 0;

	virtual void Refresh() = 0;

	virtual void SetEnabled(const InstallFolder& folder, bool enabled) = 0;

	virtual void Move(std::size_t fromIndex, std::size_t toIndex) = 0;

	virtual bool Apply(const LoadOrder& order) = 0;
};

inline IOrderService::~IOrderService() = default;
}
