#include "gui/screens/catalog/CatalogQuery.h"

#include <algorithm>
#include <cctype>

namespace wgrd::gui {
std::string CatalogQuery::Fold_(const std::string_view text) {
	std::string folded(text);
	std::ranges::transform(folded, folded.begin(), [](const unsigned char character) {
		return static_cast<char>(std::tolower(character));
	});
	return folded;
}

std::vector<const domain::CatalogRow*> CatalogQuery::Select(
	const std::vector<domain::CatalogRow>& rows, const std::string_view search,
	const std::size_t filter, const CatalogSort sort) {
	const std::string needle = Fold_(search);
	std::vector<const domain::CatalogRow*> selected;
	for (const domain::CatalogRow& row : rows) {
		if ((filter == 1 && !row.installed) || (filter == 2 && !row.manifestHeld)) {
			continue;
		}
		if (!needle.empty() && Fold_(row.modName).find(needle) == std::string::npos
			&& Fold_(row.publisher).find(needle) == std::string::npos
			&& Fold_(row.identifier).find(needle) == std::string::npos) {
			continue;
		}
		selected.push_back(&row);
	}
	std::ranges::stable_sort(selected, [sort](const domain::CatalogRow* left, const domain::CatalogRow* right) {
		if (sort == CatalogSort::Updates && left->Outdated() != right->Outdated()) {
			return left->Outdated();
		}
		if (sort == CatalogSort::Size && left->manifestHeld != right->manifestHeld) {
			return left->manifestHeld;
		}
		if (sort == CatalogSort::Size && left->totalBytes != right->totalBytes) {
			return left->totalBytes > right->totalBytes;
		}
		const std::string leftName = Fold_(left->modName);
		const std::string rightName = Fold_(right->modName);
		return leftName == rightName ? left->identifier < right->identifier : leftName < rightName;
	});
	return selected;
}
}
