#pragma once

#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

template <typename T>
class CConsumerBlockingQueue
{
public:
	CConsumerBlockingQueue<T>& operator=(const CConsumerBlockingQueue<T>&) = delete;
	CConsumerBlockingQueue(const CConsumerBlockingQueue<T>&) = delete;

	explicit CConsumerBlockingQueue(size_t maxSize = std::numeric_limits<size_t>::max());
	void push(const T& item);
	// Non-blocking
	bool try_pop(T& item);
	// Blocking
	bool pop(T& receiver, const uint32_t timeout_ms = std::numeric_limits<uint32_t>::max());

	// This method is needed for shutdown - to wake up all the threads that wait on this queue
	void wakeAllThreads();

	size_t size() const;
	bool empty() const;

private:
	const size_t              _maxSize;
	mutable std::mutex        _mutex;
	std::condition_variable   _cond;
	std::deque<T>             _queue;
};

// This method is needed for shutdown - to wake up all the threads that wait on this queue
template <typename T>
void CConsumerBlockingQueue<T>::wakeAllThreads()
{
	_cond.notify_all();
}

template <typename T>
CConsumerBlockingQueue<T>::CConsumerBlockingQueue(size_t maxSize) : _maxSize(maxSize)
{
}

template <typename T>
size_t CConsumerBlockingQueue<T>::size() const
{
	std::lock_guard <std::mutex> locker(_mutex);
	return _queue.size();
}

template <typename T>
bool CConsumerBlockingQueue<T>::empty() const
{
	std::lock_guard <std::mutex> locker(_mutex);
	return _queue.empty();
}

// Non-blocking access
template <typename T>
bool CConsumerBlockingQueue<T>::try_pop(T& item)
{
	std::lock_guard<std::mutex> locker(_mutex);
	if (_queue.empty())
		return false;

	item = std::move(_queue.front());
	_queue.pop_front();
	return true;
}

// Blocking access
template <typename T>
bool CConsumerBlockingQueue<T>::pop(T& receiver, const uint32_t timeout_ms)
{
	std::unique_lock<std::mutex> lock(_mutex);
	if (_queue.empty())
		_cond.wait_for(lock, std::chrono::milliseconds(timeout_ms));

	if (!_queue.empty())
	{
		receiver = std::move(_queue.front());
		_queue.pop_front();
		return true;
	}

	return false;
}

template <typename T>
void CConsumerBlockingQueue<T>::push(const T& item)
{
	while (_queue.size() > _maxSize) // Block until there's space in queue. Dangerous?
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

	{
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_queue.push_back(item);
		}

		_cond.notify_one();
	}
}
