#pragma once

#include "domain/types/preferences/WindowPreferences.h"

namespace wgrd::domain {
class IWindowPreferences {
public:
	virtual ~IWindowPreferences() = 0;

	[[nodiscard]] virtual WindowPreferences Load() const = 0;

	[[nodiscard]] virtual bool Save(const WindowPreferences& preferences) const = 0;
};

inline IWindowPreferences::~IWindowPreferences() = default;
}
