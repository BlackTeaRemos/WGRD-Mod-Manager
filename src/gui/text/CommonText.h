#pragma once

#include <string_view>

namespace wgrd::gui::text::common {
constexpr std::string_view NONE = "-";
constexpr std::string_view ON = "on";
constexpr std::string_view OFF = "off";
constexpr std::string_view YES = "yes";
constexpr std::string_view NO = "no";
constexpr std::string_view OK = "ok";
constexpr std::string_view WRITABLE = "writable";
constexpr std::string_view READ_ONLY = "read only";
constexpr std::string_view GAME_MISSING = "game not found";
constexpr std::string_view NO_INSTALLATION = "no game installation detected";

constexpr std::string_view BYTES = "B";
constexpr std::string_view KIBIBYTES = "KiB";
constexpr std::string_view MEBIBYTES = "MiB";
constexpr std::string_view GIBIBYTES = "GiB";

constexpr std::string_view WHOLE_UNIT_FORMAT = "{} {}";
constexpr std::string_view SCALED_UNIT_FORMAT = "{:.1f} {}";
constexpr std::string_view VERSION_FORMAT = "v{}";
constexpr std::string_view COUNT_FORMAT = "{}";
constexpr std::string_view ELLIPSIS = "...";
constexpr std::string_view POSITION_FORMAT = "{:02}";
}
