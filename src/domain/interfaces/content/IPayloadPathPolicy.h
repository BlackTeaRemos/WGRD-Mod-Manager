#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace wgrd::domain {

enum class PayloadPathError {
    Empty,
    TooLong,
    Absolute,
    Traversal,
    AlternateStream,
    ControlCharacter,
    EmptyComponent,
    TrailingDotOrSpace,
    ReservedDeviceName,
    ExtensionNotAllowed,
    MetadataOutOfPlace
};

class IPayloadPathPolicy {
public:
    virtual ~IPayloadPathPolicy() = 0;

    [[nodiscard]] virtual std::expected<std::string, PayloadPathError> Normalise(
        std::string_view path) const = 0;
};

inline IPayloadPathPolicy::~IPayloadPathPolicy() = default;

}
