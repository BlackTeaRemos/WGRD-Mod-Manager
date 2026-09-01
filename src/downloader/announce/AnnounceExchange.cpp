#include "downloader/announce/AnnounceExchange.h"

#include "downloader/announce/AnnounceWireCodec.h"

#include <algorithm>
#include <utility>

namespace wgrd::downloader {
AnnounceExchange::AnnounceExchange(
	domain::IAnnounceCatalogue& catalogue,
	domain::IAnnounceReceiver& receiver
)
	: _catalogue(&catalogue)
	, _receiver(&receiver)
	, _guard()
	, _budgets()
	, _status() {
	_status.running = true;
}

AnnounceExchange::~AnnounceExchange() = default;

std::vector<domain::AnnounceSummary> AnnounceExchange::Holdings() const {
	const std::scoped_lock lock(_guard);

	std::vector<domain::AnnounceSummary> summaries = _catalogue->Summaries();

	if (summaries.size() > AnnounceWireCodec::MAXIMUM_ENTRIES) {
		summaries.resize(AnnounceWireCodec::MAXIMUM_ENTRIES);
	}

	return summaries;
}

std::vector<domain::AnnounceSummary> AnnounceExchange::Missing(
	const std::span<const domain::AnnounceSummary> offered
) const {
	const std::scoped_lock lock(_guard);

	std::vector<domain::AnnounceSummary> wanted;

	for (const domain::AnnounceSummary& summary : offered) {
		if (wanted.size() >= AnnounceWireCodec::MAXIMUM_ENTRIES) {
			break;
		}

		if (!_catalogue->WouldAccept(summary.publisher, summary.modName, summary.version)) {
			continue;
		}

		wanted.push_back(summary);
	}

	return wanted;
}

std::optional<std::vector<std::uint8_t>> AnnounceExchange::Serve(
	const domain::AnnounceSummary& wanted
) {
	const std::scoped_lock lock(_guard);

	auto record = _catalogue->Record(wanted.publisher, wanted.modName);
	if (!record.has_value()) {
		return std::nullopt;
	}

	++_status.recordsSent;

	return record;
}

bool AnnounceExchange::PeerBlocked(const std::string& peer) const {
	const std::scoped_lock lock(_guard);

	const auto known = _budgets.find(peer);
	if (known == _budgets.end()) {
		return false;
	}

	return known->second.Blocked(PeerAnnounceBudget::Clock::now());
}

PeerAnnounceBudget* AnnounceExchange::EnsureBudget_(
	const std::string& peer,
	const PeerAnnounceBudget::Clock::time_point now
) {
	const auto known = _budgets.find(peer);
	if (known != _budgets.end()) {
		return &known->second;
	}

	if (_budgets.size() >= MAXIMUM_TRACKED_PEERS) {
		auto victim = _budgets.end();

		for (auto candidate = _budgets.begin(); candidate != _budgets.end(); ++candidate) {
			if (candidate->second.Blocked(now)) {
				continue;
			}

			if (victim == _budgets.end()) {
				victim = candidate;
				continue;
			}

			const bool candidateClean = !candidate->second.Escalated();
			const bool victimClean = !victim->second.Escalated();

			if (candidateClean != victimClean) {
				if (candidateClean) {
					victim = candidate;
				}
				continue;
			}

			if (candidate->second.LastSeen() < victim->second.LastSeen()) {
				victim = candidate;
			}
		}

		if (victim == _budgets.end()) {
			return nullptr;
		}

		_budgets.erase(victim);
	}

	return &_budgets[peer];
}

void AnnounceExchange::Penalise(const std::string& peer) {
	const std::scoped_lock lock(_guard);

	const auto now = PeerAnnounceBudget::Clock::now();

	PeerAnnounceBudget* budget = EnsureBudget_(peer, now);
	if (budget != nullptr) {
		budget->Penalise(now);
	}

	++_status.protocolViolations;
}

bool AnnounceExchange::Ingest(const std::string& peer, const std::span<const std::uint8_t> record) {
	const std::scoped_lock lock(_guard);

	++_status.recordsReceived;

	const auto now = PeerAnnounceBudget::Clock::now();

	PeerAnnounceBudget* budget = EnsureBudget_(peer, now);
	if (budget == nullptr) {
		++_status.peersThrottled;
		return false;
	}

	if (!budget->Consume(now)) {
		++_status.peersThrottled;
		return false;
	}

	const auto accepted = _receiver->Accept(record);
	if (!accepted.has_value()) {
		++_status.recordsRejected;
		return false;
	}

	++_status.recordsAccepted;

	return true;
}

void AnnounceExchange::PeerOpened() {
	const std::scoped_lock lock(_guard);
	++_status.peers;
}

void AnnounceExchange::PeerClosed() {
	const std::scoped_lock lock(_guard);

	if (_status.peers > 0) {
		--_status.peers;
	}
}

void AnnounceExchange::CountOfferSent() {
	const std::scoped_lock lock(_guard);
	++_status.offersSent;
}

void AnnounceExchange::CountOfferReceived() {
	const std::scoped_lock lock(_guard);
	++_status.offersReceived;
}

domain::GossipStatus AnnounceExchange::Snapshot() const {
	const std::scoped_lock lock(_guard);
	return _status;
}

void AnnounceExchange::UseListenPort(const std::uint16_t port) {
	const std::scoped_lock lock(_guard);
	_listenPort = port;
}

std::uint16_t AnnounceExchange::ListenPort() const {
	const std::scoped_lock lock(_guard);
	return _listenPort;
}

void AnnounceExchange::NotePeer(std::string address, const std::uint16_t port) {
	if (address.empty() || port == 0) {
		return;
	}

	const std::scoped_lock lock(_guard);

	if (_noted.size() >= MAXIMUM_TRACKED_PEERS) {
		return;
	}

	const std::pair<std::string, std::uint16_t> candidate{std::move(address), port};

	if (std::ranges::find(_noted, candidate) != _noted.end()) {
		return;
	}

	_noted.push_back(candidate);
}

std::vector<std::pair<std::string, std::uint16_t>> AnnounceExchange::TakeNotedPeers() {
	const std::scoped_lock lock(_guard);

	std::vector<std::pair<std::string, std::uint16_t>> taken;
	taken.swap(_noted);

	return taken;
}

void AnnounceExchange::RequestRefresh() {
	const std::scoped_lock lock(_guard);
	_refreshGeneration += 1;
}

std::uint64_t AnnounceExchange::RefreshGeneration() const {
	const std::scoped_lock lock(_guard);
	return _refreshGeneration;
}
}
