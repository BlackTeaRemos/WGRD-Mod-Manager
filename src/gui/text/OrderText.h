#pragma once

#include <string_view>

namespace wgrd::gui::text::order {
constexpr std::string_view TITLE = "LOAD ORDER";
constexpr std::string_view HEADING = "Load order";
constexpr std::string_view DETAILS = "DETAILS";
constexpr std::string_view NOTHING_SELECTED = "select a mod";
constexpr std::string_view NO_MANIFEST = "no mod json";
constexpr std::string_view NOT_INSTALLED = "folder not present";
constexpr std::string_view UNSIGNED = "UNSIGNED";
constexpr std::string_view FIELD_VERSION = "VERSION";
constexpr std::string_view FIELD_AUTHOR = "AUTHOR";
constexpr std::string_view FIELD_BUILDS = "BUILDS";
constexpr std::string_view FIELD_PACKS = "PACKS";
constexpr std::string_view FIELD_REVISION = "REVISION";
constexpr std::string_view FIELD_FOLDER = "FOLDER";
constexpr std::string_view ORDER_FILE = "ORDER FILE";

constexpr std::string_view DRAG_HANDLE = "::";
constexpr std::string_view MOVE_UP = "up";
constexpr std::string_view MOVE_DOWN = "dn";

constexpr std::string_view CONTEXT_FORMAT = "{} entries - {} enabled";
constexpr std::string_view ATTENTION_FORMAT = "Attention - {}";

constexpr std::string_view DEFAULT_BANNER = "DEFAULT PROFILE ACTIVE";
constexpr std::string_view DEFAULT_BANNER_DETAIL = "adding mods may corrupt";
constexpr std::string_view DEFAULT_BANNER_ADVICE = "save a named profile";
constexpr std::string_view LOCATION_FORMAT = "{}/{}";
}
