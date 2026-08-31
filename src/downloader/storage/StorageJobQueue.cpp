#include "downloader/storage/StorageJobQueue.h"

#include <utility>

namespace wgrd::downloader {
StorageJobQueue::StorageJobQueue()
	: _guard()
	, _signal()
	, _jobs()
	, _ready()
	, _stopping(false)
	, _worker() {
	_worker = std::thread([this]() {
			RunWorker_();
		}
	);
}

StorageJobQueue::~StorageJobQueue() {
	Stop();
}

void StorageJobQueue::Enqueue(std::function<void()> job) {
	const std::scoped_lock lock(_guard);
	_jobs.push_back(std::move(job));
}

void StorageJobQueue::Submit() {
	{
		const std::scoped_lock lock(_guard);

		if (_jobs.empty()) {
			return;
		}

		while (!_jobs.empty()) {
			_ready.push_back(std::move(_jobs.front()));
			_jobs.pop_front();
		}
	}

	_signal.notify_one();
}

void StorageJobQueue::RunWorker_() {
	while (true) {
		std::function<void()> job;

		{
			std::unique_lock<std::mutex> lock(_guard);

			_signal.wait(lock, [this]() {
				             return _stopping || !_ready.empty();
			             }
			);

			if (_stopping && _ready.empty()) {
				return;
			}

			job = std::move(_ready.front());
			_ready.pop_front();
		}

		job();
	}
}

void StorageJobQueue::Stop() {
	{
		const std::scoped_lock lock(_guard);
		_stopping = true;
	}

	_signal.notify_all();

	if (_worker.joinable()) {
		_worker.join();
	}
}
}
