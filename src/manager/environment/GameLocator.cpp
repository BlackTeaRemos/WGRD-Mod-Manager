#include "manager/environment/GameLocator.h"

#include <Windows.h>

#include <fstream>
#include <string>
#include <system_error>

namespace wgrd::manager {

namespace {

constexpr std::string_view GAME_DIRECTORY_NAME = "Wargame Red Dragon";
constexpr std::string_view MODS_DIRECTORY_NAME = "Mods";
constexpr std::string_view ORDER_FILE_NAME = "load_order.txt";
constexpr std::string_view GAME_EXECUTABLE_NAME = "WarGame3.exe";

std::optional<std::string> ReadRegistryString(HKEY hive, const wchar_t* subKey, const wchar_t* valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(hive, subKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    wchar_t buffer[MAX_PATH]{};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    const LSTATUS status = RegQueryValueExW(
        key,
        valueName,
        nullptr,
        &type,
        reinterpret_cast<LPBYTE>(buffer),
        &size);

    RegCloseKey(key);

    if (status != ERROR_SUCCESS || type != REG_SZ) {
        return std::nullopt;
    }

    const std::wstring wide(buffer);
    return std::filesystem::path(wide).string();
}

std::vector<std::string> ExtractQuotedValues(const std::string& contents, std::string_view key) {
    std::vector<std::string> values;

    const std::string needle = "\"" + std::string(key) + "\"";
    std::size_t cursor = 0;

    while ((cursor = contents.find(needle, cursor)) != std::string::npos) {
        cursor += needle.size();

        const std::size_t openingQuote = contents.find('"', cursor);
        if (openingQuote == std::string::npos) {
            break;
        }

        const std::size_t closingQuote = contents.find('"', openingQuote + 1);
        if (closingQuote == std::string::npos) {
            break;
        }

        values.push_back(contents.substr(openingQuote + 1, closingQuote - openingQuote - 1));
        cursor = closingQuote + 1;
    }

    return values;
}

std::string UnescapeBackslashes(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\\' && index + 1 < value.size() && value[index + 1] == '\\') {
            result.push_back('\\');
            ++index;
            continue;
        }
        result.push_back(value[index]);
    }

    return result;
}

}

std::optional<domain::GameInstallation> GameLocator::FromRoot(const std::filesystem::path& root) {
    std::error_code probe;

    const std::filesystem::path modsDirectory = root / std::filesystem::path(MODS_DIRECTORY_NAME);
    if (!std::filesystem::is_directory(modsDirectory, probe) || probe) {
        return std::nullopt;
    }

    domain::GameInstallation installation{
        root,
        modsDirectory,
        modsDirectory / std::filesystem::path(ORDER_FILE_NAME)
    };

    return installation;
}

std::optional<domain::GameInstallation> GameLocator::Resolve() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return std::nullopt;
    }

    const std::filesystem::path executable(buffer);
    const std::filesystem::path root = executable.parent_path();

    std::error_code probe;
    if (!std::filesystem::exists(root / std::filesystem::path(GAME_EXECUTABLE_NAME), probe) || probe) {
        return std::nullopt;
    }

    return FromRoot(root);
}

std::vector<domain::GameInstallation> GameLocator::Detect() {
    std::vector<domain::GameInstallation> found;

    const std::optional<std::filesystem::path> steamRoot = SteamRoot_();
    if (!steamRoot) {
        return found;
    }

    for (const std::filesystem::path& library : LibraryRoots_(*steamRoot)) {
        const std::filesystem::path candidate =
            library / "steamapps" / "common" / std::filesystem::path(GAME_DIRECTORY_NAME);

        std::error_code probe;
        if (!std::filesystem::is_directory(candidate, probe) || probe) {
            continue;
        }

        if (const std::optional<domain::GameInstallation> installation = FromRoot(candidate)) {
            found.push_back(*installation);
        }
    }

    return found;
}

std::optional<std::filesystem::path> GameLocator::SteamRoot_() {
    if (const std::optional<std::string> current =
            ReadRegistryString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath")) {
        return std::filesystem::path(*current);
    }

    if (const std::optional<std::string> machine =
            ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath")) {
        return std::filesystem::path(*machine);
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> GameLocator::LibraryRoots_(const std::filesystem::path& steamRoot) {
    std::vector<std::filesystem::path> roots;
    roots.push_back(steamRoot);

    const std::filesystem::path manifest = steamRoot / "steamapps" / "libraryfolders.vdf";

    std::ifstream input(manifest, std::ios::binary);
    if (!input) {
        return roots;
    }

    const std::string contents(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    for (const std::string& value : ExtractQuotedValues(contents, "path")) {
        roots.emplace_back(UnescapeBackslashes(value));
    }

    return roots;
}

}
