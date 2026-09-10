#include "manager/baseline/BaselineIndexService.h"

#include "domain/rules/BaselinePackLayout.h"
#include "manager/baseline/BackgroundThreadMode.h"
#include "manager/baseline/BaselineFileScanner.h"

#include <algorithm>
#include <map>
#include <utility>

namespace wgrd::manager {
BaselineIndexService::BaselineIndexService(
	const std::filesystem::path& gameRoot,
	std::filesystem::path recordFolder,
	const domain::IContentChunker& chunker,
	const domain::IContentHasher& hasher
)
	: _packRoot(domain::BaselinePackLayout::PackRoot(gameRoot))
	, _store(std::move(recordFolder), hasher)
	, _indexer(chunker, hasher)
	, _catalogue()
	, _guard()
	, _wake()
	, _queue()
	, _pending()
	, _priority()
	, _status()
	, _waiters(0)
	, _restored(false)
	, _stopping(false)
	, _worker() {}

BaselineIndexService::~BaselineIndexService() {
	Stop();
}

void BaselineIndexService::Start() {
	{
		const std::scoped_lock lock(_guard);

		if (_status.running) {
			return;
		}

		_status = domain::BaselineIndexStatus{};
		_status.running = true;
		_restored = false;
		_stopping = false;
		_queue.clear();
		_pending.clear();
		_priority.clear();
		_catalogue.Clear();
	}

	if (_worker.joinable()) {
		_worker.join();
	}

	_worker = std::thread([this]() {
			Run_();
		}
	);
}

void BaselineIndexService::Stop() {
	{
		const std::scoped_lock lock(_guard);
		_stopping = true;
	}

	_wake.notify_all();

	if (_worker.joinable()) {
		_worker.join();
	}
}

const domain::IHeldChunkCatalogue& BaselineIndexService::Catalogue() const {
	return _catalogue;
}

domain::BaselineIndexStatus BaselineIndexService::Status() const {
	const std::scoped_lock lock(_guard);

	domain::BaselineIndexStatus status = _status;
	status.boosted = _waiters > 0;

	return status;
}

std::string BaselineIndexService::NormalisePath_(const std::string_view path) {
	std::string normalised(path);
	for (char& character : normalised) {
		if (character == '\\') {
			character = '/';
		} else if (character >= 'A' && character <= 'Z') {
			character = static_cast<char>(character + ('a' - 'A'));
		}
	}
	return normalised;
}

bool BaselineIndexService::AllIndexed_(const std::set<std::string>& wanted) const {
	if (!_restored) {
		return false;
	}

	return std::ranges::none_of(wanted, [this](const std::string& relativePath) {
		                            return _pending.contains(relativePath);
	                            }
	);
}

bool BaselineIndexService::AwaitCounterparts(
	const std::span<const std::string> payloadPaths,
	const std::function<bool()>& abandon
) {
	std::set<std::string> wanted;
	for (const std::string& path : payloadPaths) {
		wanted.insert(NormalisePath_(path));
	}

	std::unique_lock lock(_guard);

	_priority.insert(wanted.begin(), wanted.end());
	Prioritise_();

	++_waiters;
	_wake.notify_all();

	bool indexed = false;

	while (true) {
		if (AllIndexed_(wanted)) {
			indexed = true;
			break;
		}

		if (_stopping || !_status.running) {
			break;
		}

		if (abandon && abandon()) {
			break;
		}

		_wake.wait_for(lock, AWAIT_POLL);
	}

	--_waiters;
	_wake.notify_all();

	lock.unlock();
	_catalogue.DiscardChanged();

	return indexed;
}

void BaselineIndexService::Restore_() {
	const std::vector<BaselineFileEntry> entries = BaselineFileScanner::Scan(_packRoot);

	std::map<std::string, BaselineFileRecord> records;
	for (BaselineFileRecord& record : _store.LoadAll()) {
		std::string relativePath = record.relativePath;
		records.try_emplace(std::move(relativePath), std::move(record));
	}

	std::deque<BaselineFileEntry> queue;
	std::set<std::string> pending;
	domain::BaselineIndexStatus status;
	status.running = true;
	status.totalFiles = entries.size();

	for (const BaselineFileEntry& entry : entries) {
		status.totalBytes += entry.stamp.size;

		const auto record = records.find(entry.relativePath);
		const bool current = record != records.end() && record->second.stamp == entry.stamp;

		if (current) {
			_catalogue.Adopt(_packRoot, record->second);
			++status.indexedFiles;
			status.indexedBytes += entry.stamp.size;
			records.erase(record);
			continue;
		}

		queue.push_back(entry);
		pending.insert(NormalisePath_(entry.relativePath));
	}

	for (const auto& [relativePath, record] : records) {
		const bool removed = _store.Remove(relativePath);
		(void)removed;
	}

	const std::scoped_lock lock(_guard);
	_queue = std::move(queue);
	_pending = std::move(pending);
	_status = status;
	_restored = true;
	Prioritise_();
}

void BaselineIndexService::Prioritise_() {
	std::ranges::stable_partition(_queue, [this](const BaselineFileEntry& entry) {
		                              return _priority.contains(NormalisePath_(entry.relativePath));
	                              }
	);
}

std::optional<BaselineIndexService::Claimed> BaselineIndexService::Claim_() {
	const std::scoped_lock lock(_guard);

	if (_stopping || _queue.empty()) {
		return std::nullopt;
	}

	Claimed claimed{_queue.front(), _waiters > 0};
	_queue.pop_front();
	_status.activeFile = claimed.entry.relativePath;
	_status.activeBytes = 0;

	return claimed;
}

void BaselineIndexService::Complete_(const BaselineFileEntry& entry, const bool indexed) {
	{
		const std::scoped_lock lock(_guard);
		_pending.erase(NormalisePath_(entry.relativePath));
		_status.activeFile.clear();
		_status.activeBytes = 0;
		if (indexed) {
			++_status.indexedFiles;
			_status.indexedBytes += entry.stamp.size;
		} else {
			++_status.failedFiles;
		}
	}

	_wake.notify_all();
}

void BaselineIndexService::Run_() {
	BackgroundThreadMode::Enter();
	bool background = true;

	Restore_();
	_wake.notify_all();

	while (true) {
		const std::optional<Claimed> claimed = Claim_();
		if (!claimed.has_value()) {
			break;
		}

		if (claimed->boosted && background) {
			BackgroundThreadMode::Leave();
			background = false;
		} else if (!claimed->boosted && !background) {
			BackgroundThreadMode::Enter();
			background = true;
		}

		const auto record = _indexer.Describe(claimed->entry, [this](const std::uint64_t bytes) {
			const std::scoped_lock lock(_guard);
			_status.activeBytes = bytes;
		});
		if (record.has_value()) {
			_catalogue.Adopt(_packRoot, *record);
			const bool saved = _store.Save(*record);
			(void)saved;
		}
		Complete_(claimed->entry, record.has_value());
	}

	{
		const std::scoped_lock lock(_guard);
		_status.running = false;
	}

	_wake.notify_all();
}
}
