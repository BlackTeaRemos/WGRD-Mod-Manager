#include "downloader/announce/PeerAnnounceBudget.h"

#include <algorithm>

namespace wgrd::downloader {
PeerAnnounceBudget::PeerAnnounceBudget()
	: _allowance(BURST_ALLOWANCE)
	, _lastRefill()
	, _blockedUntil()
	, _nextBlock(FIRST_BLOCK)
	, _consecutiveOverage(0)
	, _lastSeen()
	, _seeded(false) {}

void PeerAnnounceBudget::Refill_(const Clock::time_point now) {
	if (!_seeded) {
		_lastRefill = now;
		_seeded = true;
		return;
	}

	const auto elapsed = now - _lastRefill;
	if (elapsed < REFILL_INTERVAL) {
		return;
	}

	const auto intervals = elapsed / REFILL_INTERVAL;
	const std::uint32_t granted = static_cast<std::uint32_t>(
		std::min<std::int64_t>(intervals, BURST_ALLOWANCE));

	_allowance = std::min(BURST_ALLOWANCE, _allowance + granted);
	_lastRefill = now;
}

void PeerAnnounceBudget::ResetLadderWhenClean_(const Clock::time_point now) {
	if (_nextBlock == FIRST_BLOCK) {
		return;
	}

	if (now - _blockedUntil >= CLEAN_INTERVAL) {
		_nextBlock = FIRST_BLOCK;
	}
}

bool PeerAnnounceBudget::Blocked(const Clock::time_point now) const {
	return now < _blockedUntil;
}

bool PeerAnnounceBudget::Consume(const Clock::time_point now) {
	_lastSeen = now;

	if (Blocked(now)) {
		return false;
	}

	ResetLadderWhenClean_(now);
	Refill_(now);

	if (_allowance == 0) {
		++_consecutiveOverage;

		if (_consecutiveOverage >= SUSTAINED_OVERAGE_THRESHOLD) {
			Penalise(now);
		}

		return false;
	}

	_consecutiveOverage = 0;
	--_allowance;

	return true;
}

void PeerAnnounceBudget::Penalise(const Clock::time_point now) {
	_lastSeen = now;

	ResetLadderWhenClean_(now);

	_blockedUntil = now + _nextBlock;

	const auto widened = _nextBlock * BLOCK_MULTIPLIER;
	_nextBlock = std::min<std::chrono::seconds>(widened, MAXIMUM_BLOCK);

	_allowance = 0;
	_consecutiveOverage = 0;
}

std::uint32_t PeerAnnounceBudget::Allowance() const {
	return _allowance;
}

bool PeerAnnounceBudget::Escalated() const {
	return _nextBlock != FIRST_BLOCK;
}

PeerAnnounceBudget::Clock::time_point PeerAnnounceBudget::LastSeen() const {
	return _lastSeen;
}
}
