#ifndef _Queue_H
#define _Queue_H

#include <mutex>
#include <optional>
#include <queue>

template<typename T>
class TQueue {
public:
	TQueue() = default;
	TQueue(const TQueue<T>&) = delete;
	TQueue& operator=(const TQueue<T>&) = delete;

	TQueue(TQueue<T>&& other)
	{
		std::lock_guard<std::mutex> lock(mutex);
		queue = std::move(other.queue);
	}

	virtual ~TQueue() {}

	typename std::queue<T>::size_type size() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return queue.size();
	}

	std::optional<T> pop()
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (queue.empty()) return {};

		T tmp = queue.front();
		queue.pop();
		return tmp;
	}

	void push(const T& i)
	{
		std::lock_guard<std::mutex> lock(mutex);
		queue.push(i);
	}

private:
	std::queue<T> queue;
	mutable std::mutex mutex;
};

#endif
