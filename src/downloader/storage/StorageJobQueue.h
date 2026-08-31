#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace wgrd::downloader {
class StorageJobQueue {
public:
	explicit StorageJobQueue();

	~StorageJobQueue();

	StorageJobQueue(const StorageJobQueue&) = delete;

	StorageJobQueue& operator=(const StorageJobQueue&) = delete;

	void Enqueue(std::function<void()> job);

	void Submit();

	void Stop();

private:
	void RunWorker_();

	mutable std::mutex _guard;
	std::condition_variable _signal;
	std::deque<std::function<void()>> _jobs;
	std::deque<std::function<void()>> _ready;
	bool _stopping;
	std::thread _worker;
};
}
