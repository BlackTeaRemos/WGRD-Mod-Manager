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
	static constexpr std::uint32_t SUSTAINED_OVERAGE_THRESHOLD = 8;
	static constexpr std::chrono::hours CLEAN_INTERVAL{1};

	PeerAnnounceBudget();

	[[nodiscard]] bool Consume(Clock::time_point now);

	[[nodiscard]] bool Blocked(Clock::time_point now) const;

	void Penalise(Clock::time_point now);

	[[nodiscard]] std::uint32_t Allowance() const;

	[[nodiscard]] bool Escalated() const;

	[[nodiscard]] Clock::time_point LastSeen() const;

private:
	void Refill_(Clock::time_point now);

	void ResetLadderWhenClean_(Clock::time_point now);

	std::uint32_t _allowance;
	Clock::time_point _lastRefill;
	Clock::time_point _blockedUntil;
	std::chrono::seconds _nextBlock;
	std::uint32_t _consecutiveOverage;
	Clock::time_point _lastSeen;
	bool _seeded;
};
}
