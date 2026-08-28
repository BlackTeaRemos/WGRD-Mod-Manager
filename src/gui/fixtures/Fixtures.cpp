#include "gui/fixtures/Fixtures.h"

#include <array>

namespace wgrd::gui::fixtures {

namespace {

constexpr std::array<CatalogRow, 8> CATALOG = {{
    {".", "Sandbox Mod", "github:Noob-Development/Sandbox-Mod-Files", "1.0.4", "17 MB", "mod", "v.131544 ok", "ENABLED", "DISABLE", false},
    {".", "Unofficial Patch", "github:TK3600/wgrd-unofficial", "1.2.2b", "5.3 MB", "mod", "v.131544 ok", "ENABLED", "DISABLE", false},
    {"#", "Wargame: Asia in Conflict", "moddb mirror - aic", "1.21", "268 MB", "mod", "v.131544 ok", "QUEUED", "CANCEL", true},
    {".", "Wargame 1991", "github:wg1991/release", "59638", "18.5 MB", "mod", "v.120912 older", "MISSING", "FETCH", false},
    {".", "Blackeagle 2nd Korean War", "extends aic 1.21", "1.3", "341 MB", "extension", "v.118330 older", "MISSING", "FETCH", false},
    {"#", "Blitz-War Community Mod", "github:blitzwar/bwcm", "2.4.1", "44 MB", "mod", "v.131544 ok", "QUEUED", "CANCEL", true},
    {".", "Sandbox Unit Cards", "extends sandbox-mod 1.0.4", "3", "30 MB", "extension", "v.131544 ok", "ENABLED", "DISABLE", false},
    {".", "Realistic But Balanced", "moddb mirror - rbb", "0.936", "12 MB", "mod", "v.131544 ok", "ENABLED", "DISABLE", false}
}};

constexpr std::array<OrderRow, 6> ORDER = {{
    {"01", "Unofficial Patch", "Mods/wgrd-unofficial", "mod", true, nullptr, nullptr, false},
    {"02", "Sandbox Mod", "Mods/sandbox-mod", "mod", true, nullptr, nullptr, false},
    {"03", "Sandbox Unit Cards", "Mods/sandbox-unit-cards", "extension", true, nullptr, nullptr, false},
    {"04", "Realistic But Balanced", "Mods/realistic-but-balanced", "mod", true, nullptr, nullptr, false},
    {"05", "Wargame 1991", "Mods/wg1991", "mod", false, "build", "no packs here", false},
    {"06", "Blackeagle 2nd Korean War", "Mods/blackeagle-2kw", "extension", false, "missing", "folder not present", true}
}};

constexpr std::array<TransferRow, 3> TRANSFERS = {{
    {"TORRENT", "Wargame: Asia in Conflict", "1.21", "48.2 MB/s", "~3 s", "reused 214 MB   fetched 54 MB   chunks 3,412 / 4,288   peers 16", "chunks verified", 0.50f, 0.26f, 0.10f},
    {"HTTPS", "Sandbox Mod", "1.0.4", "2.1 MB/s", "done", "reused -   fetched 17 MB   files 11   source github release", "sig ok", 0.0f, 1.0f, 0.0f},
    {"SEEDING", "Realistic But Balanced", "0.936", "u 11.6 MB/s", "-", "seeded 12 MB   uploaded 1.2 GB   chunks 188 / 188   leechers 9", "ratio 3.42", 1.0f, 0.0f, 0.0f}
}};

constexpr std::array<ProfileRow, 4> PROFILES = {{
    {"Sunday co-op", "ACTIVE", "4 enabled - pinned", "LAUNCH"},
    {"Ladder", "READY", "no entries - vanilla", "SWITCH"},
    {"AIC campaign", "MISSING FILES", "2 entries - 1 folder missing", "FETCH MISSING"},
    {"1991 archived", "READY", "2 entries - pinned", "SWITCH"}
}};

constexpr std::array<AttentionItem, 2> ATTENTION = {{
    {"Missing folder - Blackeagle 2nd Korean War", "The order names Mods/blackeagle-2kw but that folder is not present. The entry is skipped at load.", "GO TO TRANSFERS", "remove entry", true},
    {"Build mismatch - Wargame 1991", "This mod ships no pack directory for the detected game build. It stays installed but will not contribute.", "SHOW BUILDS", "remove entry", false}
}};

constexpr std::array<FileCheckRow, 5> FILE_CHECKS = {{
    {"Mods/wgrd-unofficial", "5.3 MB", "ok", true},
    {"Mods/sandbox-mod", "17 MB", "ok", true},
    {"Mods/sandbox-unit-cards", "30 MB", "ok", true},
    {"Mods/realistic-but-balanced", "12 MB", "ok", true},
    {"Mods/blackeagle-2kw", "-", "absent", false}
}};

}

std::span<const CatalogRow> Catalog() {
    return CATALOG;
}

std::span<const OrderRow> Order() {
    return ORDER;
}

std::span<const TransferRow> Transfers() {
    return TRANSFERS;
}

std::span<const ProfileRow> Profiles() {
    return PROFILES;
}

std::span<const AttentionItem> Attention() {
    return ATTENTION;
}

std::span<const FileCheckRow> FileChecks() {
    return FILE_CHECKS;
}

}
