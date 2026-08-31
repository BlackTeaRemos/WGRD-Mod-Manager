#include "downloader/transfer/PresentHashSet.h"

#include <utility>

namespace wgrd::downloader {
PresentHashSet::PresentHashSet()
	: _guard()
	, _hashes() {}

bool PresentHashSet::Contains(const std::string& infoHash) const {
	const std::scoped_lock lock(_guard);
	return _hashes.contains(infoHash);
}

void PresentHashSet::Record(std::string infoHash) {
	const std::scoped_lock lock(_guard);
	_hashes.insert(std::move(infoHash));
}

void PresentHashSet::Forget(const std::string& infoHash) {
	const std::scoped_lock lock(_guard);

	const auto match = _hashes.find(infoHash);
	if (match != _hashes.end()) {
		_hashes.erase(match);
	}
}
}
