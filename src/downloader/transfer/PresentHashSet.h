#pragma once

#include <mutex>
#include <set>
#include <string>

namespace wgrd::downloader {
class PresentHashSet {
public:
	explicit PresentHashSet();

	PresentHashSet(const PresentHashSet&) = delete;

	PresentHashSet& operator=(const PresentHashSet&) = delete;

	[[nodiscard]] bool Contains(const std::string& infoHash) const;

	void Record(std::string infoHash);

	void Forget(const std::string& infoHash);

private:
	mutable std::mutex _guard;
	std::multiset<std::string> _hashes;
};
}
