#include "manager/payload/PayloadPathPolicy.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace wgrd::manager {

namespace {

constexpr std::array<std::string_view, 22> RESERVED_NAMES = {
    "CON", "PRN", "AUX", "NUL",
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
};

bool IsControlCharacter(char character) {
    return static_cast<unsigned char>(character) < 0x20;
}

}

PayloadPathPolicy::PayloadPathPolicy() = default;

PayloadPathPolicy::~PayloadPathPolicy() = default;

std::string PayloadPathPolicy::ToUpper_(std::string_view text) {
    std::string upper(text);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return upper;
}

bool PayloadPathPolicy::IsReservedDeviceName_(std::string_view component) {
    const std::size_t dot = component.find('.');
    const std::string_view stem = dot == std::string_view::npos ? component : component.substr(0, dot);
    const std::string upper = ToUpper_(stem);

    return std::find(RESERVED_NAMES.begin(), RESERVED_NAMES.end(), upper) != RESERVED_NAMES.end();
}

bool PayloadPathPolicy::HasPayloadExtension_(std::string_view component) {
    if (component.size() <= PAYLOAD_EXTENSION.size()) {
        return false;
    }

    const std::string_view tail = component.substr(component.size() - PAYLOAD_EXTENSION.size());
    return ToUpper_(tail) == ToUpper_(PAYLOAD_EXTENSION);
}

std::expected<void, domain::PayloadPathError> PayloadPathPolicy::ValidateComponent_(
    std::string_view component) {

    if (component.empty()) {
        return std::unexpected(domain::PayloadPathError::EmptyComponent);
    }

    if (component == "." || component == "..") {
        return std::unexpected(domain::PayloadPathError::Traversal);
    }

    if (component.back() == '.' || component.back() == ' ') {
        return std::unexpected(domain::PayloadPathError::TrailingDotOrSpace);
    }

    if (IsReservedDeviceName_(component)) {
        return std::unexpected(domain::PayloadPathError::ReservedDeviceName);
    }

    return {};
}

std::expected<std::string, domain::PayloadPathError> PayloadPathPolicy::Normalise(
    std::string_view path) const {

    if (path.empty()) {
        return std::unexpected(domain::PayloadPathError::Empty);
    }

    if (path.size() > PATH_LIMIT) {
        return std::unexpected(domain::PayloadPathError::TooLong);
    }

    std::string normalised(path);
    std::replace(normalised.begin(), normalised.end(), '\\', '/');

    for (const char character : normalised) {
        if (IsControlCharacter(character)) {
            return std::unexpected(domain::PayloadPathError::ControlCharacter);
        }

        if (character == ':') {
            return std::unexpected(domain::PayloadPathError::AlternateStream);
        }
    }

    if (normalised.front() == '/') {
        return std::unexpected(domain::PayloadPathError::Absolute);
    }

    std::vector<std::string_view> components;
    std::size_t start = 0;
    while (start <= normalised.size()) {
        const std::size_t separator = normalised.find('/', start);
        const std::size_t stop = separator == std::string::npos ? normalised.size() : separator;

        components.emplace_back(std::string_view(normalised).substr(start, stop - start));

        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }

    for (const std::string_view component : components) {
        const auto validated = ValidateComponent_(component);
        if (!validated.has_value()) {
            return std::unexpected(validated.error());
        }
    }

    const std::string_view leaf = components.back();

    if (ToUpper_(leaf) == ToUpper_(METADATA_NAME)) {
        if (components.size() != 1) {
            return std::unexpected(domain::PayloadPathError::MetadataOutOfPlace);
        }
        return normalised;
    }

    if (!HasPayloadExtension_(leaf)) {
        return std::unexpected(domain::PayloadPathError::ExtensionNotAllowed);
    }

    return normalised;
}

}
