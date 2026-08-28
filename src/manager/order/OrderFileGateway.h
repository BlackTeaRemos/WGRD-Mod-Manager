#pragma once

#include "domain/types/order/LoadOrder.h"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace wgrd::manager {
enum class OrderFileError {
	NotFound
	, ReadFailed
	, WriteFailed
	, RenameFailed
	, MalformedEntry
};

class OrderFileGateway {
public:
	[[nodiscard]] static std::string Serialize(const domain::LoadOrder& order);

	[[nodiscard]] static std::expected<domain::LoadOrder, OrderFileError> Parse(std::string_view contents);

	[[nodiscard]] static std::expected<domain::LoadOrder, OrderFileError> Read(const std::filesystem::path& path);

	[[nodiscard]] static std::expected<void, OrderFileError> Write(
		const std::filesystem::path& path,
		const domain::LoadOrder& order
	);

private:
	static std::string_view StripLineEnding_(std::string_view line) noexcept;
	static bool IsIgnoredLine_(std::string_view line) noexcept;
};
}
