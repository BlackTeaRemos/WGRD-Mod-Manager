#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace wgrd::downloader {
enum class TorrentCacheError {
	KeyRejected
	, EntryMissing
	, EntryOversized
	, EntryChanged
	, PayloadRejected
	, WriteFailed
};

class TorrentCache {
public:
	static constexpr std::string_view SUFFIX = ".torrent";
	static constexpr std::string_view STAGE_MARK = ".partial.";
	static constexpr std::size_t MAXIMUM_BYTES = 64u * 1024u * 1024u;

	explicit TorrentCache(std::filesystem::path folder);

	[[nodiscard]] std::expected<std::vector<char>, TorrentCacheError> Load(std::string_view key) const;

	[[nodiscard]] std::expected<void, TorrentCacheError> Save(
		std::string_view key,
		const std::vector<char>& bencoded
	) const;

	std::size_t Prune(const std::vector<std::string>& liveKeys) const;

private:
	[[nodiscard]] static bool IsAcceptableKey_(std::string_view key);

	[[nodiscard]] static std::string StageToken_();

	[[nodiscard]] std::filesystem::path PathFor_(std::string_view key) const;

	std::filesystem::path _folder;
};
}
