#pragma once

#include "domain/interfaces/content/IPayloadPathPolicy.h"

#include <expected>
#include <string>
#include <string_view>

namespace wgrd::manager {
class PayloadPathPolicy final : public domain::IPayloadPathPolicy {
public:
	static constexpr std::size_t PATH_LIMIT = 240;
	static constexpr std::string_view METADATA_NAME = "mod.json";
	static constexpr std::string_view PAYLOAD_EXTENSION = ".dat";

	PayloadPathPolicy();

	~PayloadPathPolicy() override;

	[[nodiscard]] std::expected<std::string, domain::PayloadPathError> Normalise(
		std::string_view path
	) const override;

private:
	static std::expected<void, domain::PayloadPathError> ValidateComponent_(std::string_view component);

	static bool IsReservedDeviceName_(std::string_view component);

	static bool HasPayloadExtension_(std::string_view component);

	static std::string ToUpper_(std::string_view text);
};
}
