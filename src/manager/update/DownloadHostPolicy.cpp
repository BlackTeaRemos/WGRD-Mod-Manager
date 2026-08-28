#include "manager/update/DownloadHostPolicy.h"

#include <algorithm>

namespace wgrd::manager {
namespace {
	constexpr char Lowered(const char character) {
		if (character >= 'A' && character <= 'Z') {
			return static_cast<char>(character - 'A' + 'a');
		}

		return character;
	}

	bool SameHost(const std::string_view candidate, const std::string_view allowed) {
		if (candidate.size() != allowed.size()) {
			return false;
		}

		for (std::size_t position = 0; position < candidate.size(); ++position) {
			if (Lowered(candidate[position]) != allowed[position]) {
				return false;
			}
		}

		return true;
	}
}

std::string_view DownloadHostPolicy::HostOf(const std::string_view url) {
	if (!url.starts_with(SCHEME)) {
		return {};
	}

	const std::string_view remainder = url.substr(SCHEME.size());
	const std::size_t boundary = remainder.find_first_of("/?#");

	return boundary == std::string_view::npos ? remainder : remainder.substr(0, boundary);
}

bool DownloadHostPolicy::Accepts(const std::string_view url) {
	const std::string_view host = HostOf(url);

	if (host.empty()) {
		return false;
	}

	if (host.find_first_of("@:") != std::string_view::npos) {
		return false;
	}

	return std::ranges::any_of(ALLOWED_HOSTS, [host](const std::string_view allowed) {
			return SameHost(host, allowed);
		}
	);
}
}
