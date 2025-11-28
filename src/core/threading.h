#pragma once

#include <atomic>

#include "../pch.h"
#include <functional>
#include <mutex>

static uint32 AtomicAdd(volatile uint32& a, uint32 b) {
	return InterlockedAdd((volatile LONG*)&a, b) - b;
}

static uint32 AtomicIncrement(volatile uint32& a) {
	return InterlockedIncrement((volatile LONG*)&a) - 1;
}

static uint64 AtomicIncrement(volatile uint64& a) {
	return InterlockedIncrement64((volatile LONG64*)&a) - 1;
}

static uint32 AtomicDecrement(volatile uint32& a) {
	return InterlockedDecrement((volatile LONG*)&a) + 1;
}

static uint64 AtomicDecrement(volatile uint64& a) {
	return InterlockedDecrement64((volatile LONG64*)&a) + 1;
}

static uint32 AtomicCompareExchange(volatile uint32& dst, uint32 exchange, uint32 compare) {
	return InterlockedCompareExchange((volatile LONG*)&dst, exchange, compare);
}

static uint64 AtomicCompareExchange(volatile uint64& dst, uint64 exchange, uint64 compare) {
	return InterlockedCompareExchange((volatile LONG*)&dst, exchange, compare);
}

struct ThreadJobContext {
	std::atomic_int32_t NumJobs = 0;

	void AddWork(const std::function<void()>& work);
	void WaitForWorkCompletion();
};

struct WorkQueueEntry {
	std::function<void()> Callback;
	ThreadJobContext* Context;
};

class JobFactory {
public:
	void InitializeJobSystem();
	static JobFactory* Instance() { return _instance; }
private:
	template<class T, uint32 capacity>
	struct ThreadSafeRingBuffer {
		bool PushBack(const T& t);
		bool PopFront(T& t);

		uint32 NextItemToRead = 0;
		uint32 NextItemToWrite = 0;
		T Data[capacity];

	private:
		std::mutex _mutex;
	};

	ThreadSafeRingBuffer<WorkQueueEntry, 256> _queue = {};
	HANDLE _semaphoreHandle = {};
	static JobFactory* _instance;

	bool PerformWork();
	void WorkerThreadProc();

	friend ThreadJobContext;
};

template<class T, uint32 capacity>
bool JobFactory::ThreadSafeRingBuffer<T, capacity>::PushBack(const T &t) {
	bool result = false;
	_mutex.lock();
	uint32 next = (NextItemToWrite + 1) % capacity;
	if (next != NextItemToRead) {
		Data[NextItemToWrite] = t;
		NextItemToWrite = next;
		result = true;
	}
	_mutex.unlock();
	return result;
}

template<class T, uint32 capacity>
bool JobFactory::ThreadSafeRingBuffer<T, capacity>::PopFront(T &t) {
	bool result = false;
	_mutex.lock();
	if (NextItemToRead != NextItemToWrite) {
		t = Data[NextItemToRead];
		NextItemToRead = (NextItemToRead + 1) % capacity;
		result = true;
	}
	_mutex.unlock();
	return result;
}
