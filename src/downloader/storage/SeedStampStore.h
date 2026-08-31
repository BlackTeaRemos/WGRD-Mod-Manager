#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace wgrd::downloader {
class SeedStampStore {
public:
	static constexpr std::string_view SUFFIX = ".stamp";
	static constexpr std::size_t MAXIMUM_BYTES = 1024u * 1024u;

	explicit SeedStampStore(std::filesystem::path folder);

	void UseFolder(std::filesystem::path folder);

	[[nodiscard]] std::optional<std::string> Load(std::string_view key) const;

	bool Save(std::string_view key, const std::string& stamp) const;

	void Forget(std::string_view key) const;

private:
	[[nodiscard]] static bool IsAcceptableKey_(std::string_view key);

	[[nodiscard]] std::filesystem::path PathFor_(std::string_view key) const;

	std::filesystem::path _folder;
};
}
