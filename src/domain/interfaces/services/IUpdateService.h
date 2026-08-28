#pragma once

#include "domain/types/status/UpdateStatus.h"

namespace wgrd::domain {
class IUpdateService {
public:
	virtual ~IUpdateService() = 0;

	[[nodiscard]] virtual UpdateStatus Status() const = 0;

	virtual void Check() = 0;

	virtual void Download() = 0;

	[[nodiscard]] virtual bool ApplyAndRestart() = 0;
};

inline IUpdateService::~IUpdateService() = default;
}
