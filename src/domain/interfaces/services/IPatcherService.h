#pragma once

#include "domain/types/status/PatcherStatus.h"

namespace wgrd::domain {
class IPatcherService {
public:
	virtual ~IPatcherService() = 0;

	[[nodiscard]] virtual PatcherStatus Status() const = 0;

	virtual void Check() = 0;

	virtual void Install() = 0;
};

inline IPatcherService::~IPatcherService() = default;
}
