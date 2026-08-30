#pragma once

#include <string_view>

namespace wgrd::manager::text {
constexpr std::string_view KEY_ALREADY_PRESENT = "key already present";
constexpr std::string_view KEY_CREATE_FAILED = "key create failed";
constexpr std::string_view KEY_CREATED = "key created and unlocked";
constexpr std::string_view KEY_CREATED_UNTRUSTED = "key created local trust failed";
constexpr std::string_view KEY_CREATED_NO_REGISTRATION = "key created registration failed";
constexpr std::string_view KEY_CREATED_NO_REVOCATION = "key created revocation failed";
constexpr std::string_view KEY_UNLOCKED = "key unlocked";
constexpr std::string_view KEY_UNLOCKED_UNTRUSTED = "key unlocked local trust failed";
constexpr std::string_view KEY_UNREADABLE = "key unreadable";
constexpr std::string_view KEY_LOCKED = "key locked";
constexpr std::string_view KEY_MISSING = "no signing key";
constexpr std::string_view PASSPHRASE_TOO_SHORT = "passphrase too short";
constexpr std::string_view PASSPHRASE_REJECTED = "passphrase rejected";
constexpr std::string_view PUBLISHER_REJECTED = "name letters digits only";
constexpr std::string_view NO_UNLOCKED_KEY = "no unlocked key";

constexpr std::string_view KEY_FILE_PRESENT = "key file already there";
constexpr std::string_view KEY_FILE_MISSING = "key file missing";
constexpr std::string_view KEY_FILE_UNREADABLE = "key file unreadable";
constexpr std::string_view KEY_FILE_UNWRITABLE = "key file unwritable";
constexpr std::string_view KEY_FILE_CORRUPT = "key file corrupt";
constexpr std::string_view KEY_GENERATION_FAILED = "key generation failed";
constexpr std::string_view PUBLISHER_NAME_REJECTED = "publisher name rejected";
constexpr std::string_view KEY_FAILED = "key failed";

constexpr std::string_view MANIFEST_BUILD_FAILED = "manifest build failed";
constexpr std::string_view MANIFEST_FOLDER_MISSING = "mod folder missing";
constexpr std::string_view MANIFEST_FOLDER_UNREADABLE = "mod folder unreadable";
constexpr std::string_view MANIFEST_FOLDER_EMPTY = "mod folder empty";
constexpr std::string_view MANIFEST_PATH_REJECTED = "payload path rejected";
constexpr std::string_view MANIFEST_FILE_UNREADABLE = "mod file unreadable";
constexpr std::string_view MANIFEST_TOO_MANY_CHUNKS = "too many chunks";
constexpr std::string_view MANIFEST_NAME_REJECTED = "mod name rejected";
constexpr std::string_view SIGN_FAILED = "sign failed";
constexpr std::string_view TORRENT_BUILD_FAILED = "torrent build failed";
constexpr std::string_view ANNOUNCE_FAILED = "announce failed";
constexpr std::string_view ANNOUNCE_REJECTED = "announce rejected";
constexpr std::string_view ANNOUNCE_MALFORMED = "announce malformed";
constexpr std::string_view ANNOUNCE_UNKNOWN_PUBLISHER = "publisher not registered";
constexpr std::string_view ANNOUNCE_REVOKED_PUBLISHER = "publisher key revoked";
constexpr std::string_view ANNOUNCE_SIGNATURE_INVALID = "announce signature invalid";
constexpr std::string_view ANNOUNCE_NOT_NEWER = "version not newer";

constexpr std::string_view PUBLISH_HASHING = "hashing mod files";
constexpr std::string_view PUBLISH_SIGNING = "signing manifest";
constexpr std::string_view PUBLISH_ANNOUNCING = "announcing release";
constexpr std::string_view MANIFEST_STORE_FAILED = "manifest store failed";
constexpr std::string_view PUBLISHED_PREFIX = "published ";

constexpr std::string_view PROFILE_NAME_REJECTED = "name letters digits only";
constexpr std::string_view PROFILE_NAME_TAKEN = "name already used";
constexpr std::string_view PROFILE_NOT_FOUND = "profile not found";
constexpr std::string_view PROFILE_WRITE_FAILED = "profile write failed";
constexpr std::string_view PROFILE_DELETE_FAILED = "profile delete failed";
constexpr std::string_view PROFILE_ORDER_FAILED = "order write failed";
constexpr std::string_view PROFILE_ACTIVE_FAILED = "active write failed";
constexpr std::string_view PROFILE_DEFAULT_KEPT = "default cannot be deleted";
constexpr std::string_view PROFILE_GAME_MISSING = "game profile missing";
constexpr std::string_view PROFILE_GAME_REJECTED = "game profile rejected";
constexpr std::string_view DEFAULT_SET = "default profile set";
constexpr std::string_view NO_INSTALLATION = "no installation detected";
constexpr std::string_view CAPTURED_PREFIX = "captured ";
constexpr std::string_view CLONED_PREFIX = "cloned ";
constexpr std::string_view ACTIVATED_PREFIX = "activated ";
constexpr std::string_view DELETED_PREFIX = "deleted ";

constexpr std::string_view REMOVAL_BUSY = "transfer in flight";
constexpr std::string_view REMOVAL_UNKNOWN = "unknown mod";
constexpr std::string_view REMOVAL_OUTSIDE = "folder outside mods";
constexpr std::string_view REMOVAL_ABSENT = "folder not present";
constexpr std::string_view REMOVAL_LOCKED = "folder locked";
constexpr std::string_view REMOVED_PREFIX = "removed ";

constexpr std::string_view INSTALL_COMPARING = "comparing against installed";
constexpr std::string_view INSTALL_FETCHING_MANIFEST = "fetching manifest from peers";
constexpr std::string_view INSTALL_MANIFEST_REFUSED = "manifest fetch refused";
constexpr std::string_view INSTALL_MANIFEST_REJECTED = "manifest rejected";
constexpr std::string_view INSTALL_FETCH_REFUSED = "fetch refused";
constexpr std::string_view INSTALL_FETCH_FAILED = "fetch failed";
constexpr std::string_view INSTALL_FETCHING_CHUNKS = "fetching missing chunks";
constexpr std::string_view INSTALL_MATERIALISING = "materialising mod folder";
constexpr std::string_view INSTALL_FROM_HELD = "materialising from held chunks";
constexpr std::string_view INSTALL_FAILED = "install failed";
constexpr std::string_view INSTALL_CANCELLED = "cancelled";
constexpr std::string_view INSTALLED_PREFIX = "installed ";

constexpr std::string_view PATCHER_CHECKING = "checking patcher";
constexpr std::string_view PATCHER_REPOSITORY_UNSET = "patcher repository unset";
constexpr std::string_view PATCHER_CHECK_FAILED = "patcher check failed";

constexpr std::string_view LOOKUP_FAILED = "lookup failed";
constexpr std::string_view LOOKUP_REPOSITORY_UNSET = "repository unset";
constexpr std::string_view LOOKUP_REQUEST_FAILED = "request failed";
constexpr std::string_view LOOKUP_RESPONSE_MALFORMED = "response malformed";
constexpr std::string_view LOOKUP_TAG_REJECTED = "tag rejected";
constexpr std::string_view LOOKUP_ASSET_MISSING = "asset missing";
constexpr std::string_view LOOKUP_ASSET_URL_REJECTED = "asset url rejected";
constexpr std::string_view PATCHER_CURRENT = "patcher current";
constexpr std::string_view PATCHER_AVAILABLE = "patcher update available";
constexpr std::string_view PATCHER_DOWNLOADING = "downloading patcher";
constexpr std::string_view PATCHER_NOT_CHECKED = "check patcher first";
constexpr std::string_view PATCHER_DOWNLOAD_FAILED = "patcher download failed";
constexpr std::string_view PATCHER_SIZE_MISMATCH = "patcher size mismatch";
constexpr std::string_view PATCHER_INSTALL_FAILED = "patcher install failed";
constexpr std::string_view PATCHER_INSTALLED = "patcher installed";
}
