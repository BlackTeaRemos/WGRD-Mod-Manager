#pragma once

#include <chrono>
#include <cstdint>

namespace wgrd::downloader {
class PeerAnnounceBudget {
public:
	using Clock = std::chrono::steady_clock;

	static constexpr std::uint32_t BURST_ALLOWANCE = 8;
	static constexpr std::chrono::seconds REFILL_INTERVAL{60};
	static constexpr std::chrono::minutes FIRST_BLOCK{1};
	static constexpr std::uint32_t BLOCK_MULTIPLIER = 5;
	static constexpr std::chrono::hours MAXIMUM_BLOCK{24};

	PeerAnnounceBudget();

	[[nodiscard]] bool Consume(Clock::time_point now);

	[[nodiscard]] bool Blocked(Clock::time_point now) const;

	void Penalise(Clock::time_point now);

	[[nodiscard]] std::uint32_t Allowance() const;

private:
	void Refill_(Clock::time_point now);

	std::uint32_t _allowance;
	Clock::time_point _lastRefill;
	Clock::time_point _blockedUntil;
	std::chrono::seconds _nextBlock;
	bool _seeded;
};
}
