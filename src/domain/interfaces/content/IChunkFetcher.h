#pragma once

#include "domain/types/content/ChunkDestination.h"
#include "domain/types/content/ChunkDigest.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace wgrd::domain {
enum class FetchPhase {
	Idle
	, Metadata
	, Downloading
	, Complete
	, Failed
};

struct FetchStatus {
	FetchPhase phase = FetchPhase::Idle;
	std::string identifier;
	std::uint64_t fetchedBytes = 0;
	std::uint64_t inFlightBytes = 0;
	std::uint64_t wantedBytes = 0;
	std::uint32_t peers = 0;
	std::uint64_t hashFailures = 0;
	std::uint64_t bannedPeers = 0;
	std::string lastFailure;
	std::filesystem::path stagingFolder;

	[[nodiscard]] bool Busy() const {
		return phase == FetchPhase::Metadata || phase == FetchPhase::Downloading;
	}
};

enum class FetchError {
	Busy
	, NothingWanted
	, MagnetRejected
	, SessionRejected
};

class IChunkFetcher {
public:
	virtual ~IChunkFetcher() = 0;

	[[nodiscard]] virtual std::expected<void, FetchError> Begin(
		std::string identifier,
		const ChunkDigest& infoHash,
		const std::filesystem::path& stagingFolder,
		const std::vector<std::string>& wantedFiles,
		const std::vector<ChunkDestination>& destinations,
		bool verifyExisting
	) = 0;

	[[nodiscard]] virtual FetchStatus Fetch() const = 0;

	virtual void Cancel() = 0;
};

inline IChunkFetcher::~IChunkFetcher() = default;
}
