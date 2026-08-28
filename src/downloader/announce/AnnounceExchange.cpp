#include "downloader/announce/AnnounceExchange.h"

#include "downloader/announce/AnnounceWireCodec.h"

namespace wgrd::downloader {
namespace {
	constexpr std::size_t MAXIMUM_TRACKED_PEERS = 512;
}

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

void AnnounceExchange::Penalise(const std::string& peer) {
	const std::scoped_lock lock(_guard);

	if (_budgets.size() >= MAXIMUM_TRACKED_PEERS && !_budgets.contains(peer)) {
		_budgets.erase(_budgets.begin());
	}

	_budgets[peer].Penalise(PeerAnnounceBudget::Clock::now());

	++_status.protocolViolations;
}

bool AnnounceExchange::Ingest(const std::string& peer, const std::span<const std::uint8_t> record) {
	const std::scoped_lock lock(_guard);

	++_status.recordsReceived;

	if (_budgets.size() >= MAXIMUM_TRACKED_PEERS && !_budgets.contains(peer)) {
		_budgets.erase(_budgets.begin());
	}

	PeerAnnounceBudget& budget = _budgets[peer];

	if (!budget.Consume(PeerAnnounceBudget::Clock::now())) {
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
}
