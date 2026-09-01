#pragma once

#include <cstddef>

namespace wgrd::domain {
struct MirrorStatus {
	bool enabled = false;
	std::size_t pending = 0;
	std::size_t claimed = 0;
};

class IMirrorService {
public:
	virtual ~IMirrorService() = 0;

	[[nodiscard]] virtual MirrorStatus Status() const = 0;

	virtual void SetEnabled(bool enabled) = 0;

	virtual void Poll() = 0;
};

inline IMirrorService::~IMirrorService() = default;
}
