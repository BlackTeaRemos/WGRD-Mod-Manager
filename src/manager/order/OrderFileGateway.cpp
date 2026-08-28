#include "manager/order/OrderFileGateway.h"

#include <fstream>
#include <system_error>

namespace wgrd::manager {

namespace {

constexpr std::string_view LINE_ENDING = "\r\n";

constexpr std::string_view HEADER =
    "# Wargame Red Dragon mod load order - one mod folder per line.\r\n"
    "# Later entries OVERRIDE earlier ones. '#' comments, blanks ignored.\r\n";

constexpr std::string_view TEMPORARY_SUFFIX = ".tmp";

}

std::string OrderFileGateway::Serialize(const domain::LoadOrder& order) {
    std::string contents(HEADER);

    for (const domain::InstallFolder& folder : order.EnabledFolders()) {
        contents.append(folder.Value());
        contents.append(LINE_ENDING);
    }

    return contents;
}

std::expected<domain::LoadOrder, OrderFileError> OrderFileGateway::Parse(std::string_view contents) {
    std::vector<domain::OrderEntry> entries;

    std::size_t position = 0;
    while (position <= contents.size()) {
        const std::size_t breakIndex = contents.find('\n', position);
        const std::size_t lineLength =
            (breakIndex == std::string_view::npos ? contents.size() : breakIndex) - position;
        const std::string_view line = StripLineEnding_(contents.substr(position, lineLength));

        if (!IsIgnoredLine_(line)) {
            const auto folder = domain::InstallFolder::Parse(line);
            if (!folder) {
                return std::unexpected(OrderFileError::MalformedEntry);
            }
            entries.push_back(domain::OrderEntry{*folder, true});
        }

        if (breakIndex == std::string_view::npos) {
            break;
        }
        position = breakIndex + 1;
    }

    return domain::LoadOrder(std::move(entries));
}

std::expected<domain::LoadOrder, OrderFileError> OrderFileGateway::Read(const std::filesystem::path& path) {
    std::error_code existenceError;
    if (!std::filesystem::exists(path, existenceError) || existenceError) {
        return std::unexpected(OrderFileError::NotFound);
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::unexpected(OrderFileError::ReadFailed);
    }

    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad()) {
        return std::unexpected(OrderFileError::ReadFailed);
    }

    return Parse(contents);
}

std::expected<void, OrderFileError> OrderFileGateway::Write(
    const std::filesystem::path& path,
    const domain::LoadOrder& order) {

    std::filesystem::path temporaryPath = path;
    temporaryPath += TEMPORARY_SUFFIX;

    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            return std::unexpected(OrderFileError::WriteFailed);
        }

        const std::string contents = Serialize(order);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.flush();

        if (!output) {
            return std::unexpected(OrderFileError::WriteFailed);
        }
    }

    std::error_code renameError;
    std::filesystem::rename(temporaryPath, path, renameError);
    if (renameError) {
        std::error_code removeError;
        std::filesystem::remove(temporaryPath, removeError);
        return std::unexpected(OrderFileError::RenameFailed);
    }

    return {};
}

std::string_view OrderFileGateway::StripLineEnding_(std::string_view line) noexcept {
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    return line;
}

bool OrderFileGateway::IsIgnoredLine_(std::string_view line) noexcept {
    return line.empty() || line.front() == '#';
}

}
