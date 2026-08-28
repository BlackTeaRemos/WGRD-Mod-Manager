#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace wgrd::domain {

enum class InstallFolderError {
    Empty,
    PathSeparator,
    CommentMarker,
    LeadingWhitespace,
    TrailingWhitespace
};

class InstallFolder {
public:
    static std::expected<InstallFolder, InstallFolderError> Parse(std::string_view text);

    [[nodiscard]] const std::string& Value() const noexcept;

    bool operator==(const InstallFolder& other) const noexcept = default;

private:
    explicit InstallFolder(std::string value);

    std::string _value;
};

}
