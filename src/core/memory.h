#pragma once

#include "../pch.h"

#define KB(n) (1024 * (n))
#define MB(n) (1024 * KB(n))
#define GB(n) (1024 * MB(n))

static uint32 AlignTo(uint32 currentOffset, uint32 alignment) {
	uint32 mask = alignment - 1;
	uint32 misalignment = currentOffset & mask;
	if (misalignment == 0) {
		return currentOffset;
	}
	uint32 adjustment = alignment - misalignment;
	return currentOffset + adjustment;
}

static uint64 AlignTo(uint64 currentOffset, uint64 alignment) {
	uint64 mask = alignment - 1;
	uint64 misalignment = currentOffset & mask;
	if (misalignment == 0) {
		return currentOffset;
	}
	uint64 adjustment = alignment - misalignment;
	return currentOffset + adjustment;
}

static void* AlignTo(void* currentAddress, uint64 alignment) {
	uint64 mask = alignment - 1;
	uint64 misalignment = (uint64)currentAddress & mask;
	if (misalignment == 0) {
		return currentAddress;
	}
	uint64 adjustment = alignment - misalignment;
	return (uint8*)currentAddress + adjustment;
}

struct MemoryBlock {
	uint8* Start;
	uint8* Current;
	uint64 Size;
	MemoryBlock* Next;
};

class MemoryArena {
public:
	void* Allocate(uint64 size, bool clearToZero = false);
	void Reset();
	void Free();

	MemoryBlock* GetFreeBlock(uint64 size = 0);

	MemoryBlock* CurrentBlock = nullptr;
	MemoryBlock* LastActiveBlock = nullptr;
	MemoryBlock* FreeBlocks = nullptr;
	uint64 MinimumBlockSize = 1;
};
