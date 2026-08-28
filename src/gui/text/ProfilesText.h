#pragma once

#include <string_view>

namespace wgrd::gui::text::profiles {
constexpr std::string_view TITLE = "PROFILES";
constexpr std::string_view UNAVAILABLE = "profiles unavailable";
constexpr std::string_view EMPTY = "no profiles captured yet";
constexpr std::string_view NO_ACTIVE = "no active profile";

constexpr std::string_view CAPTURE_LABEL = "SAVE CURRENT PROFILE AS";
constexpr std::string_view NAME_FIELD = "profilename";
constexpr std::string_view CAPTURE = "SAVE";
constexpr std::string_view SELECT = "SELECT";
constexpr std::string_view FORCE_SELECT = "FORCE SELECT";
constexpr std::string_view CLONE = "CLONE";
constexpr std::string_view DELETE_PROFILE = "DELETE";

constexpr std::string_view LOAD_ORDER = "LOAD ORDER";
constexpr std::string_view UNREADABLE = "profile unreadable";
constexpr std::string_view NOTHING_ENABLED = "nothing enabled";
constexpr std::string_view NOT_INSTALLED = "not installed";

constexpr std::string_view GAME_PROFILES = "GAME PROFILES";
constexpr std::string_view NO_GAME_PROFILES = "no game profiles found";
constexpr std::string_view ACCOUNT_FORMAT = "account {}";
constexpr std::string_view SET_DEFAULT = "SET DEFAULT";
constexpr std::string_view LIVE_MARK = "live";
constexpr std::string_view PROFILE_FILE_HELD = "profile file held";
constexpr std::string_view PROFILE_FILE_ABSENT = "no profile file";

constexpr std::string_view BADGE_ACTIVE = "ACTIVE";
constexpr std::string_view BADGE_MISSING = "MISSING";

constexpr std::string_view ACTIVE_FORMAT = "active {}";
constexpr std::string_view ENABLED_OF_FORMAT = "{} enabled of {}";
constexpr std::string_view MISSING_FORMAT = "{} enabled folders not installed";
constexpr std::string_view POSITION_FORMAT = "{:02}";
}
