#pragma once

#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "domain/interfaces/trust/IAnnounceReceiver.h"
#include "domain/types/status/GossipStatus.h"
#include "downloader/announce/PeerAnnounceBudget.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace wgrd::downloader {
class AnnounceExchange {
public:
	static constexpr std::size_t MAXIMUM_TRACKED_PEERS = 512;

	AnnounceExchange(
		domain::IAnnounceCatalogue& catalogue,
		domain::IAnnounceReceiver& receiver
	);

	~AnnounceExchange();

	AnnounceExchange(const AnnounceExchange&) = delete;
	AnnounceExchange& operator=(const AnnounceExchange&) = delete;

	[[nodiscard]] std::vector<domain::AnnounceSummary> Holdings() const;

	[[nodiscard]] std::vector<domain::AnnounceSummary> Missing(
		std::span<const domain::AnnounceSummary> offered
	) const;

	[[nodiscard]] std::optional<std::vector<std::uint8_t>> Serve(
		const domain::AnnounceSummary& wanted
	);

	[[nodiscard]] bool Ingest(const std::string& peer, std::span<const std::uint8_t> record);

	[[nodiscard]] bool PeerBlocked(const std::string& peer) const;

	void Penalise(const std::string& peer);

	void PeerOpened();

	void PeerClosed();

	void CountOfferSent();

	void CountOfferReceived();

	[[nodiscard]] domain::GossipStatus Snapshot() const;

private:
	[[nodiscard]] PeerAnnounceBudget* EnsureBudget_(
		const std::string& peer,
		PeerAnnounceBudget::Clock::time_point now
	);

	domain::IAnnounceCatalogue* _catalogue;
	domain::IAnnounceReceiver* _receiver;

	mutable std::mutex _guard;
	std::map<std::string, PeerAnnounceBudget> _budgets;
	domain::GossipStatus _status;
};
}
