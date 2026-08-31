#include "downloader/storage/SeedAttestations.h"

#include <utility>

namespace wgrd::downloader {
SeedAttestations::SeedAttestations()
	: _guard()
	, _attested() {}

void SeedAttestations::Mark(std::string torrentName) {
	const std::scoped_lock lock(_guard);
	_attested.insert(std::move(torrentName));
}

void SeedAttestations::Forget(const std::string_view torrentName) {
	const std::scoped_lock lock(_guard);

	const auto match = _attested.find(torrentName);
	if (match != _attested.end()) {
		_attested.erase(match);
	}
}

bool SeedAttestations::Attests(const std::string_view torrentName) const {
	const std::scoped_lock lock(_guard);
	return _attested.find(torrentName) != _attested.end();
}

std::size_t SeedAttestations::Count() const {
	const std::scoped_lock lock(_guard);
	return _attested.size();
}
}
