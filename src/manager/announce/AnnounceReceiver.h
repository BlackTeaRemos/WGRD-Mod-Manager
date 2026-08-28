#pragma once

#include "domain/interfaces/trust/IAnnounceCatalogue.h"
#include "domain/interfaces/trust/IAnnounceReceiver.h"
#include "domain/interfaces/trust/IKeyRegistry.h"
#include "manager/announce/AnnounceStore.h"

#include <cstdint>
#include <expected>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::manager {
class AnnounceReceiver final
		: public domain::IAnnounceReceiver,
		  public domain::IAnnounceCatalogue {
public:
	AnnounceReceiver(const domain::IKeyRegistry& registry, const AnnounceStore* store);

	~AnnounceReceiver() override;

	std::size_t Restore();

	[[nodiscard]] std::expected<domain::SignedAnnounce, domain::AnnounceRejection> Accept(
		std::span<const std::uint8_t> record
	) override;

	[[nodiscard]] std::vector<domain::AnnounceSummary> Summaries() const override;

	[[nodiscard]] std::optional<std::vector<std::uint8_t>> Record(
		const domain::PublisherFingerprint& publisher,
		std::string_view modName
	) const override;

	[[nodiscard]] bool WouldAccept(
		const domain::PublisherFingerprint& publisher,
		std::string_view modName,
		std::uint64_t version
	) const override;

	[[nodiscard]] std::optional<domain::SignedAnnounce> Retained(const std::string& identifier) const;

	[[nodiscard]] std::vector<domain::SignedAnnounce> All() const;

	[[nodiscard]] std::size_t RetainedCount() const;

	[[nodiscard]] std::size_t RejectedCount() const;

private:
	[[nodiscard]] static std::string IdentifierOf_(
		const domain::PublisherFingerprint& publisher,
		std::string_view modName
	);

	const domain::IKeyRegistry* _registry;
	const AnnounceStore* _store;
	mutable std::mutex _guard;
	std::map<std::string, domain::SignedAnnounce> _retained;
	std::size_t _rejected;
};
}
