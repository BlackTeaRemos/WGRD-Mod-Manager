#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace wgrd::domain {
class IFilePicker {
public:
	virtual ~IFilePicker() = 0;

	[[nodiscard]] virtual std::optional<std::filesystem::path> SaveFile(
		std::string_view title,
		std::string_view suggestedName,
		std::string_view filterLabel,
		std::string_view filterPattern
	) const = 0;

	[[nodiscard]] virtual std::optional<std::filesystem::path> OpenFile(
		std::string_view title,
		std::string_view filterLabel,
		std::string_view filterPattern
	) const = 0;

	[[nodiscard]] virtual std::optional<std::filesystem::path> PickFolder(
		std::string_view title
	) const = 0;
};

inline IFilePicker::~IFilePicker() = default;
}
