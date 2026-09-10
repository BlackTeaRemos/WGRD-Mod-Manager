#include "domain/rules/BaselinePackLayout.h"

namespace wgrd::domain {
std::filesystem::path BaselinePackLayout::PackRoot(const std::filesystem::path& gameRoot) {
	return gameRoot / std::filesystem::path(PACK_ROOT);
}

std::filesystem::path BaselinePackLayout::CounterpartFor(
	const std::filesystem::path& gameRoot,
	const std::string_view payloadPath
) {
	return PackRoot(gameRoot) / std::filesystem::path(payloadPath);
}
}
