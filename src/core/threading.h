#pragma once

#include "../pch.h"

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

struct ThreadMutex {
	HANDLE Handle;

	static ThreadMutex Create() {
		ThreadMutex result = { CreateMutex(nullptr, FALSE, nullptr) };
		return result;
	}

	bool Lock() {
		return WaitForSingleObject(Handle, INFINITE) == WAIT_OBJECT_0;
	}

	bool Unlock() {
		return ReleaseMutex(Handle);
	}
};
