#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace wgrd::downloader {
class TransferBudget {
public:
	static constexpr std::string_view FILE_NAME = "transfer.budget";
	static constexpr std::int64_t UNLIMITED = 0;
	static constexpr std::int64_t MAXIMUM_BYTES_PER_SECOND = 10ll * 1024ll * 1024ll * 1024ll;

	TransferBudget();

	void UseFolder(std::filesystem::path folder);

	[[nodiscard]] std::optional<std::int64_t> Load() const;

	void Save(std::int64_t bytesPerSecond) const;

private:
	std::filesystem::path _folder;
};
}
