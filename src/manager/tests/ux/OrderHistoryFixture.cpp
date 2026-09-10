#include "manager/tests/ux/OrderHistoryFixture.h"

#include <chrono>
#include <string>
#include <system_error>

namespace wgrd::manager::tests {
OrderHistoryFixture::OrderHistoryFixture()
	: _root(std::filesystem::temp_directory_path() / "wgrd-order-ux"
		/ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) {
	std::filesystem::create_directories(_root / "Mods" / "alpha");
	std::filesystem::create_directories(_root / "Mods" / "beta");
}

OrderHistoryFixture::~OrderHistoryFixture() {
	std::error_code failure;
	std::filesystem::remove_all(_root, failure);
}

const std::filesystem::path& OrderHistoryFixture::Root() const {
	return _root;
}

domain::GameInstallation OrderHistoryFixture::Installation() const {
	return domain::GameInstallation{_root, _root / "Mods", _root / "Mods" / "load_order.txt"};
}
}
