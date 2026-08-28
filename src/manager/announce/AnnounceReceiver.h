#pragma once

#include "domain/interfaces/trust/IAnnounceReceiver.h"
#include "domain/interfaces/trust/IKeyRegistry.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace wgrd::manager {

class AnnounceReceiver final : public domain::IAnnounceReceiver {
public:
    explicit AnnounceReceiver(const domain::IKeyRegistry& registry);

    ~AnnounceReceiver() override;

    [[nodiscard]] std::expected<domain::SignedAnnounce, domain::AnnounceRejection> Accept(
        std::span<const std::uint8_t> record) override;

    [[nodiscard]] std::optional<domain::SignedAnnounce> Retained(const std::string& identifier) const;

    [[nodiscard]] std::vector<domain::SignedAnnounce> All() const;

    [[nodiscard]] std::size_t RetainedCount() const;

    [[nodiscard]] std::size_t RejectedCount() const;

private:
    const domain::IKeyRegistry* _registry;
    std::map<std::string, domain::SignedAnnounce> _retained;
    std::size_t _rejected;
};

}
