#pragma once

#include <mutex>
#include <set>
#include <string>
#include <string_view>

namespace wgrd::downloader {
class SeedAttestations {
public:
	SeedAttestations();

	SeedAttestations(const SeedAttestations&) = delete;

	SeedAttestations& operator=(const SeedAttestations&) = delete;

	void Mark(std::string torrentName);

	void Forget(std::string_view torrentName);

	[[nodiscard]] bool Attests(std::string_view torrentName) const;

	[[nodiscard]] std::size_t Count() const;

private:
	mutable std::mutex _guard;
	std::set<std::string, std::less<>> _attested;
};
}
