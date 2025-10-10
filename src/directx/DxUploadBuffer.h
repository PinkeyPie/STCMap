#pragma once

#include <mutex>

#include "dx.h"
#include "../pch.h"
#include "../core/memory.h"
#include "../core/threading.h"

struct DxAllocation {
	void* CpuPtr;
	D3D12_GPU_VIRTUAL_ADDRESS GpuPtr;
};

struct DxPage {
	DxResource Buffer;
	DxPage* Next;

	uint8* CpuBasePtr;
	D3D12_GPU_VIRTUAL_ADDRESS GpuBasePtr;

	uint64 PageSize;
	uint64 CurrentOffset;
};

class DxPagePool {
public:
	DxPagePool() = default;

	MemoryArena Arena;
	DxDevice Device;

	std::mutex Mutex = {};

	uint64 PageSize = 0;
	DxPage* FreePages = nullptr;
	DxPage* UsedPages = nullptr;
	DxPage* LastUsedPage = nullptr;

	DxPage* GetFreePage();
	void ReturnPage(DxPage* page);
	void Reset();

private:
	DxPage* AllocateNewPage();
};

class DxUploadBuffer {
public:
	DxPagePool* PagePool;
	DxPage* CurrentPage;

	DxAllocation Allocate(uint64 size, uint64 alignment);
	void Reset();
};

