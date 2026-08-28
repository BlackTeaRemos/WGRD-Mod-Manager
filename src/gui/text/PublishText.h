#pragma once

#include <string_view>

namespace wgrd::gui::text::publish {
constexpr std::string_view TITLE = "PUBLISH RELEASE";
constexpr std::string_view UNAVAILABLE = "publishing unavailable";
constexpr std::string_view NO_KEY = "no signing key yet";
constexpr std::string_view PUBLISHING_AS_FORMAT = "publishing as {}";

constexpr std::string_view STEP_CREATE = "1 CREATE KEY";
constexpr std::string_view STEP_SOURCE = "2 SOURCE";
constexpr std::string_view STEP_SIGN = "3 UNLOCK AND SIGN";

constexpr std::string_view PUBLISHER_NAME = "PUBLISHER NAME";
constexpr std::string_view PASSPHRASE = "PASSPHRASE";
constexpr std::string_view NAME_FIELD = "publisher";
constexpr std::string_view CREATE_PASSPHRASE_FIELD = "createpassphrase";
constexpr std::string_view UNLOCK_PASSPHRASE_FIELD = "unlockpassphrase";

constexpr std::string_view CREATE_KEY = "CHOOSE LOCATION AND CREATE";
constexpr std::string_view NAME_RULE = "name letters digits dot underscore hyphen";

constexpr std::string_view MOD_FOLDER = "MOD FOLDER";
constexpr std::string_view CANDIDATE_LOCATION_FORMAT = "Mods/{}";
constexpr std::string_view NO_CANDIDATES = "no mod folders found";

constexpr std::string_view SIGNING_KEY = "SIGNING KEY";
constexpr std::string_view UNLOCKED = "UNLOCKED";
constexpr std::string_view LOCK_KEY = "LOCK KEY";
constexpr std::string_view UNLOCK_KEY = "SELECT KEY FILE AND UNLOCK";

constexpr std::string_view PREFLIGHT_READY = "PRE-FLIGHT";
constexpr std::string_view PREFLIGHT_INCOMPLETE = "PRE-FLIGHT";
constexpr std::string_view CHECK_KEY = "signing key unlocked";
constexpr std::string_view CHECK_FOLDER = "mod folder selected";
constexpr std::string_view SIGN_AND_ANNOUNCE = "SIGN AND ANNOUNCE";

constexpr std::string_view HISTORY = "PUBLISHED THIS SESSION";
constexpr std::string_view HISTORY_EMPTY = "nothing published yet";
constexpr std::string_view CHUNKS_FILES_FORMAT = "{} chunks - {} files";

constexpr std::string_view PICKER_STORE_KEY = "Store signing key";
constexpr std::string_view PICKER_SELECT_KEY = "Select signing key";
constexpr std::string_view KEY_FILTER_LABEL = "signing key";
constexpr std::string_view KEY_FILTER_PATTERN = "*.wgrdkey";
constexpr std::string_view KEY_SUGGESTED_NAME = "publisher.wgrdkey";

constexpr std::string_view PICKER_UNAVAILABLE = "file dialog unavailable";
constexpr std::string_view NO_LOCATION = "no location chosen";
constexpr std::string_view NO_KEY_FILE = "no key file chosen";
}
