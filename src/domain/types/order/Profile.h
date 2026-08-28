#pragma once

#include "domain/types/order/LoadOrder.h"

#include <string>
#include <utility>

namespace wgrd::domain {
class Profile {
public:
	Profile() = default;

	Profile(std::string name, LoadOrder order)
		: _name(std::move(name))
		, _order(std::move(order))
		, _account() {}

	Profile(std::string name, LoadOrder order, std::string account)
		: _name(std::move(name))
		, _order(std::move(order))
		, _account(std::move(account)) {}

	[[nodiscard]] const std::string& Name() const noexcept {
		return _name;
	}

	[[nodiscard]] const LoadOrder& Order() const noexcept {
		return _order;
	}

	[[nodiscard]] const std::string& Account() const noexcept {
		return _account;
	}

private:
	std::string _name;
	LoadOrder _order;
	std::string _account;
};
}
