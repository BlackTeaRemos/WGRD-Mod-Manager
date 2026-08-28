#pragma once

#include <string_view>

namespace wgrd::gui::text::catalog {
constexpr std::string_view TITLE = "CATALOG";
constexpr std::string_view UNAVAILABLE = "catalog unavailable";
constexpr std::string_view NO_DATA_DIRECTORY = "no data directory";
constexpr std::string_view EMPTY = "empty";

constexpr std::string_view FILTER_ALL = "all";
constexpr std::string_view FILTER_INSTALLED = "installed";
constexpr std::string_view FILTER_HELD = "manifest held";
constexpr std::string_view REFRESH = "refresh";

constexpr std::string_view COLUMN_MOD = "MOD";
constexpr std::string_view COLUMN_VERSION = "VERSION";
constexpr std::string_view COLUMN_SIZE = "SIZE";
constexpr std::string_view COLUMN_CHUNKS = "CHUNKS";
constexpr std::string_view COLUMN_FILES = "FILES";
constexpr std::string_view COLUMN_STATE = "STATE";

constexpr std::string_view INSTALLED_MARK = "*";
constexpr std::string_view ABSENT_MARK = "-";
constexpr std::string_view STATE_INSTALLED = "INSTALLED";
constexpr std::string_view STATE_UNSIGNED = "UNSIGNED";
constexpr std::string_view REVOKED_NOTE = "publisher key revoked";
constexpr std::string_view STATE_ANNOUNCED = "ANNOUNCED";
constexpr std::string_view MANIFEST_ON_DEMAND = "manifest on demand";

constexpr std::string_view ACTION_INSTALL = "install";
constexpr std::string_view ACTION_UPDATE = "update";
constexpr std::string_view ACTION_CURRENT = "up to date";

constexpr std::string_view MENU_DELETE = "delete from disk";
constexpr std::string_view MENU_CONFIRM = "confirm delete";
constexpr std::string_view MENU_NOT_INSTALLED = "not installed";
constexpr std::string_view MENU_BUSY = "transfer in flight";

constexpr std::string_view CONTEXT_FORMAT = "{} announced - {} keys registered - {} rejected";
constexpr std::string_view CHUNKS_FORMAT = "{} chunks";
constexpr std::string_view FILES_FORMAT = "{} files";
}
