#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wgrd::domain {

class GameBuild {
public:
    static std::optional<GameBuild> Parse(std::string_view text);

    explicit GameBuild(std::uint32_t value) noexcept;

    [[nodiscard]] std::uint32_t Value() const noexcept;

    [[nodiscard]] std::string ToText() const;

    bool operator==(const GameBuild& other) const noexcept = default;
    auto operator<=>(const GameBuild& other) const noexcept = default;

private:
    std::uint32_t _value;
};

}
