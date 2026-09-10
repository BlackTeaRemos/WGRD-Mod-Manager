#pragma once

#include "domain/interfaces/content/IHeldChunkCatalogue.h"
#include "domain/types/status/BaselineIndexStatus.h"

#include <functional>
#include <span>
#include <string>

namespace wgrd::domain {
class IBaselineIndexService {
public:
	virtual ~IBaselineIndexService() = 0;

	virtual void Start() = 0;

	virtual void Stop() = 0;

	[[nodiscard]] virtual bool AwaitCounterparts(
		std::span<const std::string> payloadPaths,
		const std::function<bool()>& abandon
	) = 0;

	[[nodiscard]] virtual BaselineIndexStatus Status() const = 0;

	[[nodiscard]] virtual const IHeldChunkCatalogue& Catalogue() const = 0;
};

inline IBaselineIndexService::~IBaselineIndexService() = default;
}
