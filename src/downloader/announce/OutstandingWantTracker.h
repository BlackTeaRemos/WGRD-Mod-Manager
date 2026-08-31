#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <string>

namespace wgrd::downloader {
class OutstandingWantTracker {
public:
	using Clock = std::chrono::steady_clock;

	static constexpr std::size_t MAXIMUM_OUTSTANDING = 128;
	static constexpr std::chrono::seconds WANT_EXPIRY{300};

	OutstandingWantTracker();

	[[nodiscard]] bool Track(const std::string& identifier, Clock::time_point now);

	[[nodiscard]] bool Redeem(const std::string& identifier);

	void Prune(Clock::time_point now);

	[[nodiscard]] std::size_t Count() const;

private:
	std::map<std::string, Clock::time_point> _pending;
};
}
