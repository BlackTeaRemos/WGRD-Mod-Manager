#pragma once

#include "domain/interfaces/content/IContentChunker.h"
#include "domain/interfaces/content/IContentHasher.h"
#include "domain/interfaces/services/IBaselineIndexService.h"
#include "manager/baseline/BaselineChunkCatalogue.h"
#include "manager/baseline/BaselineFileEntry.h"
#include "manager/baseline/records/BaselineRecordStore.h"
#include "manager/baseline/worker/BaselineFileIndexer.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>

namespace wgrd::manager {
class BaselineIndexService final : public domain::IBaselineIndexService {
public:
	static constexpr std::chrono::milliseconds AWAIT_POLL{100};

	BaselineIndexService(
		const std::filesystem::path& gameRoot,
		std::filesystem::path recordFolder,
		const domain::IContentChunker& chunker,
		const domain::IContentHasher& hasher
	);

	BaselineIndexService(const BaselineIndexService&) = delete;

	BaselineIndexService& operator=(const BaselineIndexService&) = delete;

	~BaselineIndexService() override;

	void Start() override;

	void Stop() override;

	[[nodiscard]] bool AwaitCounterparts(
		std::span<const std::string> payloadPaths,
		const std::function<bool()>& abandon
	) override;

	[[nodiscard]] domain::BaselineIndexStatus Status() const override;

	[[nodiscard]] const domain::IHeldChunkCatalogue& Catalogue() const override;

private:
	struct Claimed {
		BaselineFileEntry entry;
		bool boosted;
	};

	void Run_();

	void Restore_();

	[[nodiscard]] std::optional<Claimed> Claim_();

	void Complete_(const BaselineFileEntry& entry, bool indexed);

	[[nodiscard]] static std::string NormalisePath_(std::string_view path);

	void Prioritise_();

	[[nodiscard]] bool AllIndexed_(const std::set<std::string>& wanted) const;

	std::filesystem::path _packRoot;
	BaselineRecordStore _store;
	BaselineFileIndexer _indexer;
	BaselineChunkCatalogue _catalogue;

	mutable std::mutex _guard;
	std::condition_variable _wake;
	std::deque<BaselineFileEntry> _queue;
	std::set<std::string> _pending;
	std::set<std::string> _priority;
	domain::BaselineIndexStatus _status;
	std::size_t _waiters;
	bool _restored;
	bool _stopping;

	std::thread _worker;
};
}
