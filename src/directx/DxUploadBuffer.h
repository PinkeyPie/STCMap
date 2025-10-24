#pragma once

#include <deque>
#include <mutex>

#include "dx.h"
#include "../pch.h"
#include "../core/memory.h"
#include "../core/threading.h"

struct DxAllocation {
	uint8* CpuPtr;
	D3D12_GPU_VIRTUAL_ADDRESS GpuPtr;
};

struct DxPage {
	DxAllocation Base = {};

	DxPage(uint64 pageSize);
	void Init(ID3D12Device* device);
	uint64 GetOffset() const { return _currentOffset; }
	bool CheckAvailableSpace(uint64 size) const { return size < _pageSize - _currentOffset; }
	DxAllocation GetAllocation(uint64 offset, uint64 alignment);

private:
	DxResource _buffer = nullptr;
	uint64 _pageSize;
	uint64 _currentOffset = 0;
};

class DxPagePool {
public:
	DxPagePool(uint64 pageSize) : _pageSize(pageSize) {}

	DxPage* LastUsedPage = nullptr;

	DxPage* GetFreePage();
	void ReturnPage(DxPage* page);
	void Reset();

private:
	std::deque<DxPage*> _freePages = {};
	std::deque<DxPage*> _usedPages = {};
	DxPage* _lastUsedPage = nullptr;
	uint64 _pageSize;

	std::mutex _mutex = {};
};

class DxUploadBuffer {
public:
	DxPagePool* PagePool;
	DxPage* CurrentPage;

	DxAllocation Allocate(uint64 size, uint64 alignment);
	void Reset();
};

