#include "memory.h"
#include <algorithm>
#include "math.h"

#ifdef _MSC_VER
struct Struct {
	
};
#endif

namespace {
	MemoryBlock* AllocateMemoryBlock(uint64 size) {
		uint64 blockSize = AlignTo(sizeof(MemoryBlock), 64);
		void* data = _aligned_malloc(blockSize + size, 64);

		auto result = static_cast<MemoryBlock*>(data);
		result->Start = (uint8*)data + blockSize;
		result->Size = size;
		return result;
	}
}

MemoryBlock* MemoryArena::GetFreeBlock(uint64 size) {
	size = Max(size, MinimumBlockSize);

	MemoryBlock* result = nullptr;

	for (MemoryBlock* block = FreeBlocks, *prev = nullptr; block; prev = block, block = block->Next) {
		if (block->Size >= size) {
			if (prev) {
				prev->Next = block->Next;
			}
			else {
				FreeBlocks = block->Next;
			}

			result = block;
			break;
		}
	}

	if (not result) {
		result = AllocateMemoryBlock(size);
	}

	result->Next = nullptr;
	result->Current = result->Start;

	return result;
}

void* MemoryArena::Allocate(uint64 size, bool clearToZero) {
	if (!CurrentBlock or CurrentBlock->Size < size) {
		MemoryBlock* block = GetFreeBlock(size);
		block->Next = CurrentBlock;

		if (not LastActiveBlock) {
			LastActiveBlock = block;
		}

		CurrentBlock = block;
	}

	void* result = CurrentBlock->Current;
	CurrentBlock->Current += size;
	CurrentBlock->Size -= size;

	if (clearToZero) {
		memset(result, 0, size);
	}

	return result;
}

void MemoryArena::Reset() {
	if (LastActiveBlock) {
		LastActiveBlock->Next = FreeBlocks;
	}
	FreeBlocks = CurrentBlock;
	CurrentBlock = nullptr;
	LastActiveBlock = nullptr;
}

void MemoryArena::Free() {
	Reset();
	for (MemoryBlock* block = FreeBlocks; block; ) {
		MemoryBlock* next = block->Next;
		_aligned_free(block);
		block = next;
	}
	*this = {};
}
