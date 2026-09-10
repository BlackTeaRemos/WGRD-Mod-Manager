#pragma once

#include <string_view>

namespace wgrd::domain {
class IOrderHistory {
public:
	virtual ~IOrderHistory() = 0;
	[[nodiscard]] virtual bool CanUndo() const = 0;
	[[nodiscard]] virtual bool Undo() = 0;
	[[nodiscard]] virtual std::string_view LastMessage() const = 0;
};

inline IOrderHistory::~IOrderHistory() = default;
}
