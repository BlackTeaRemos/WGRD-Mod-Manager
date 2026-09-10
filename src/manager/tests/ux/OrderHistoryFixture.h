#pragma once

#include "domain/types/game/GameInstallation.h"

#include <filesystem>

namespace wgrd::manager::tests {
class OrderHistoryFixture {
public:
	OrderHistoryFixture();
	~OrderHistoryFixture();
	[[nodiscard]] const std::filesystem::path& Root() const;
	[[nodiscard]] domain::GameInstallation Installation() const;

private:
	std::filesystem::path _root;
};
}
