#include "downloader/torrent/build/TorrentCache.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <process.h>
#include <random>
#include <system_error>
#include <utility>

namespace wgrd::downloader {
TorrentCache::TorrentCache(std::filesystem::path folder)
	: _folder(std::move(folder)) {}

bool TorrentCache::IsAcceptableKey_(const std::string_view key) {
	if (key.empty() || key.size() > 128) {
		return false;
	}

	return std::ranges::all_of(key, [](const char character) {
			return (character >= '0' && character <= '9')
			       || (character >= 'a' && character <= 'f')
			       || (character >= 'A' && character <= 'F');
		}
	);
}

std::string TorrentCache::StageToken_() {
	std::random_device entropy;
	const std::uint64_t nonce =
			(static_cast<std::uint64_t>(entropy()) << 32) | static_cast<std::uint64_t>(entropy());

	return std::format("{}-{:016x}", _getpid(), nonce);
}

std::filesystem::path TorrentCache::PathFor_(const std::string_view key) const {
	return _folder / (std::string(key) + std::string(SUFFIX));
}

std::expected<std::vector<char>, TorrentCacheError> TorrentCache::Load(const std::string_view key) const {
	if (!IsAcceptableKey_(key)) {
		return std::unexpected(TorrentCacheError::KeyRejected);
	}

	const std::filesystem::path source = PathFor_(key);

	std::error_code failure;
	const std::uintmax_t declared = std::filesystem::file_size(source, failure);

	if (failure || declared == 0) {
		return std::unexpected(TorrentCacheError::EntryMissing);
	}

	if (declared > MAXIMUM_BYTES) {
		return std::unexpected(TorrentCacheError::EntryOversized);
	}

	std::ifstream input(source, std::ios::binary);
	if (!input) {
		return std::unexpected(TorrentCacheError::EntryMissing);
	}

	std::vector<char> bencoded(static_cast<std::size_t>(declared));
	input.read(bencoded.data(), static_cast<std::streamsize>(bencoded.size()));

	if (input.gcount() != static_cast<std::streamsize>(bencoded.size())) {
		return std::unexpected(TorrentCacheError::EntryChanged);
	}

	if (input.peek() != std::char_traits<char>::eof()) {
		return std::unexpected(TorrentCacheError::EntryOversized);
	}

	return bencoded;
}

std::expected<void, TorrentCacheError> TorrentCache::Save(
	const std::string_view key,
	const std::vector<char>& bencoded
) const {
	if (!IsAcceptableKey_(key)) {
		return std::unexpected(TorrentCacheError::KeyRejected);
	}

	if (bencoded.empty() || bencoded.size() > MAXIMUM_BYTES) {
		return std::unexpected(TorrentCacheError::PayloadRejected);
	}

	std::error_code failure;
	std::filesystem::create_directories(_folder, failure);

	if (failure) {
		return std::unexpected(TorrentCacheError::WriteFailed);
	}

	const std::filesystem::path target = PathFor_(key);
	const std::filesystem::path staged =
			std::filesystem::path(target.string() + std::string(STAGE_MARK) + StageToken_());

	{
		std::ofstream output(staged, std::ios::binary | std::ios::trunc);
		if (!output) {
			return std::unexpected(TorrentCacheError::WriteFailed);
		}

		output.write(bencoded.data(), static_cast<std::streamsize>(bencoded.size()));

		if (!output) {
			output.close();
			std::filesystem::remove(staged, failure);
			return std::unexpected(TorrentCacheError::WriteFailed);
		}
	}

	failure.clear();
	std::filesystem::rename(staged, target, failure);

	if (failure) {
		std::filesystem::remove(staged, failure);
		return std::unexpected(TorrentCacheError::WriteFailed);
	}

	return {};
}

std::size_t TorrentCache::Prune(const std::vector<std::string>& liveKeys) const {
	std::error_code failure;
	std::filesystem::directory_iterator entries(_folder, failure);

	if (failure) {
		return 0;
	}

	std::size_t removedCount = 0;

	for (const std::filesystem::directory_entry& entry : entries) {
		if (!entry.is_regular_file(failure)) {
			continue;
		}

		const std::filesystem::path& candidate = entry.path();

		if (candidate.extension().string() != SUFFIX) {
			continue;
		}

		const std::string stem = candidate.stem().string();

		if (!IsAcceptableKey_(stem)) {
			continue;
		}

		if (std::ranges::find(liveKeys, stem) != liveKeys.end()) {
			continue;
		}

		if (std::filesystem::remove(candidate, failure)) {
			removedCount += 1;
		}
	}

	return removedCount;
}
}
