#pragma once

#include <string_view>

namespace wgrd::gui::text::shell {
constexpr std::string_view PRODUCT = "WGRD MOD MANAGER";
constexpr std::wstring_view WINDOW_TITLE = L"WGRD MOD MANAGER";

constexpr std::string_view WORKSPACE = "WORKSPACE";
constexpr std::string_view CHUNK_STORE = "CHUNK STORE";

constexpr std::string_view CATALOG = "Catalog";
constexpr std::string_view LOAD_ORDER = "Load Order";
constexpr std::string_view TRANSFERS = "Transfers";
constexpr std::string_view PROFILES = "Profiles";
constexpr std::string_view PUBLISH = "Publish";

constexpr std::string_view SETTINGS = "SETTINGS";
constexpr std::string_view CLOSE_GLYPH = "x";
constexpr std::string_view MAXIMISE_GLYPH = "[]";
constexpr std::string_view MINIMISE_GLYPH = "-";

constexpr std::string_view SEPARATOR_AT = " @ ";
constexpr std::string_view LOCAL_BUILD = "local build";

constexpr std::string_view MANAGER_LABEL = "MANAGER";
constexpr std::string_view PATCHER_LABEL = "PATCHER";
constexpr std::string_view CHECK = "CHECK";
constexpr std::string_view UPDATE = "UPDATE";
constexpr std::string_view INSTALL = "INSTALL";
constexpr std::string_view RESTART = "RESTART";
constexpr std::string_view NOT_INSTALLED = "not installed";
constexpr std::string_view UNKNOWN_VERSION = "unknown";

constexpr std::string_view STAMP_FORMAT = "v{} {}";
constexpr std::string_view INDEX_FORMAT = "{} @ {}";
constexpr std::string_view TRUST_FORMAT = "{} keys - {} announced";
constexpr std::string_view RATE_FORMAT = "d {}/s u {}/s";
constexpr std::string_view SEEDING_FORMAT = "seeding {}";
constexpr std::string_view PEERS_FORMAT = "{} peers";
constexpr std::string_view ENABLED_OF_FORMAT = "{}/{}";
constexpr std::string_view COUNT_FORMAT = "{}";
}
