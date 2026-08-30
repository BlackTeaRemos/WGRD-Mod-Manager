#pragma once

#include <string_view>

namespace wgrd::gui::text::modal {
constexpr std::string_view SETTINGS = "Settings";
constexpr std::string_view DONE = "DONE";
constexpr std::string_view CLOSE_GLYPH = "x";

constexpr std::string_view GAME_LOCATION = "GAME LOCATION";
constexpr std::string_view NO_INSTALLATION = "no installation detected";
constexpr std::string_view PLACE_BESIDE_GAME = "place beside the game";
constexpr std::string_view INSTALLED_FORMAT = "{} mods installed";

constexpr std::string_view TRANSFER = "TRANSFER";
constexpr std::string_view SEEDING = "seeding";
constexpr std::string_view SEEDING_HINT = "serves installed mods";
constexpr std::string_view TRANSPORT_NOTE = "bittorrent trackerless dht lan";
constexpr std::string_view CHUNKING_NOTE = "fastcdc chunking avg 4 MiB";

constexpr std::string_view UPDATES = "UPDATES";
constexpr std::string_view UPDATES_UNAVAILABLE = "updates unavailable";
constexpr std::string_view CHECK_UPDATES = "CHECK FOR UPDATES";
constexpr std::string_view DOWNLOAD_UPDATE = "DOWNLOAD UPDATE";
constexpr std::string_view RESTART_UPDATE = "RESTART AND UPDATE";
constexpr std::string_view INSTALLED_VERSION_FORMAT = "installed {}";
constexpr std::string_view VERSION_PAIR_FORMAT = "installed {} - latest {}";
constexpr std::string_view DOWNLOAD_PROGRESS_FORMAT = "{:.1f} of {:.1f} MiB";

constexpr std::string_view TRUST_REGISTRY = "TRUST REGISTRY";
constexpr std::string_view REGISTRY_UNAVAILABLE = "registry unavailable";
constexpr std::string_view SYNC_REGISTRY = "SYNC REGISTRY";
constexpr std::string_view KEYS_TRUSTED_FORMAT = "{} keys trusted";
constexpr std::string_view SYNCED_FORMAT = "{} - synced {}s ago";

constexpr std::string_view DETAIL_GONE = "no longer announced";
constexpr std::string_view DETAIL_GONE_BODY = "this announce is no longer retained";
constexpr std::string_view DETAIL_META_FORMAT = "v{} - {} - {} chunks - {} files";
constexpr std::string_view DETAIL_BODY_FIRST = "Installs as a folder under Mods/";
constexpr std::string_view DETAIL_BODY_SECOND = "pins the version";

constexpr std::string_view IDENTITY = "IDENTITY";
constexpr std::string_view FIELD_MOD = "MOD";
constexpr std::string_view FIELD_PUBLISHER = "PUBLISHER";
constexpr std::string_view FIELD_INSTALLED = "INSTALLED";

constexpr std::string_view SIGNATURE = "SIGNATURE";
constexpr std::string_view MANIFEST_VERIFIED = "manifest verified - ed25519";
constexpr std::string_view MANIFEST_ABSENT = "manifest not held yet";

constexpr std::string_view ACTION_INSTALL = "INSTALL";
constexpr std::string_view ACTION_REINSTALL = "REINSTALL";
constexpr std::string_view ACTION_VERIFY = "VERIFY";
}
