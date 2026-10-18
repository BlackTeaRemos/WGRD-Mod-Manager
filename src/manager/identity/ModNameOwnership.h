#pragma once

#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "manager/install/InstalledReleaseStore.h"

#include <algorithm>
#include <string_view>

namespace wgrd::manager {
class ModNameOwnership {
public:
	[[nodiscard]] static bool SameName(const std::string_view left, const std::string_view right) {
		return std::ranges::equal(left, right, [](char leftCharacter, char rightCharacter) {
			if (leftCharacter >= 'A' && leftCharacter <= 'Z') {
				leftCharacter = static_cast<char>(leftCharacter + ('a' - 'A'));
			}
			if (rightCharacter >= 'A' && rightCharacter <= 'Z') {
				rightCharacter = static_cast<char>(rightCharacter + ('a' - 'A'));
			}
			return leftCharacter == rightCharacter;
		});
	}

	[[nodiscard]] static bool Conflicts(const std::string_view name, const std::string_view identifier,
		const domain::IAnnounceCatalogue& catalogue, const InstalledReleaseStore* installed) {
		for (const auto& summary : catalogue.Summaries()) {
			if (SameName(name, summary.modName) && summary.Identifier() != identifier) {
				return true;
			}
		}
		if (installed != nullptr) {
			for (const auto& release : installed->LoadAll()) {
				if (SameName(name, release.modName) && release.identifier != identifier) {
					return true;
				}
			}
		}
		return false;
	}
};
}
