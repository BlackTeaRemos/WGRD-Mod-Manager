#include "domain/types/game/GameBuild.h"

#include <charconv>

namespace wgrd::domain {

std::optional<GameBuild> GameBuild::Parse(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::uint32_t parsed = 0;
    const char* const first = text.data();
    const char* const last = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);

    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }

    return GameBuild(parsed);
}

GameBuild::GameBuild(std::uint32_t value) noexcept
    : _value(value) {
}

std::uint32_t GameBuild::Value() const noexcept {
    return _value;
}

std::string GameBuild::ToText() const {
    return std::to_string(_value);
}

}
