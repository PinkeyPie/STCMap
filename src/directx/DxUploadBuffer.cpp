#include "DxUploadBuffer.h"

#include "DxContext.h"
#include "../core/memory.h"

DxPage::DxPage(uint64 pageSize) : _pageSize(pageSize) {}

void DxPage::Init(ID3D12Device* device) {
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(_pageSize);
	auto heapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ThrowIfFailed(device->CreateCommittedResource(
		&heapDesc,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_buffer.GetAddressOf())
	));

	Base.GpuPtr = _buffer->GetGPUVirtualAddress();
	_buffer->Map(0, nullptr, (void**)&Base.CpuPtr);
}

DxPage* DxPagePool::GetFreePage() {
	_mutex.lock();
	DxPage* page = nullptr;

	if (!_freePages.empty()) {
		page = _freePages.front();
		_freePages.pop_front();
	}
	_mutex.unlock();

	if (not page) {
		DxDevice device = DxContext::Instance().GetDevice();
		page = new DxPage{_pageSize};
		page->Init(device.Get());
	}

	return page;
}

void DxPagePool::ReturnPage(DxPage* page) {
	std::lock_guard lock(_mutex);
	_usedPages.push_back(page);
	if (!LastUsedPage) {
		LastUsedPage = page;
	}
}

void DxPagePool::Reset() {
	LastUsedPage = nullptr;
	std::ranges::copy(_usedPages, _freePages.begin());
	_usedPages.clear();
}

DxAllocation DxPage::GetAllocation(uint64 size, uint64 alignment) {
	DxAllocation result;

	uint64 offset = _currentOffset == 0 ? AlignTo(_currentOffset, alignment) : 0;
	result.CpuPtr = Base.CpuPtr + offset;
	result.GpuPtr = Base.GpuPtr + offset;

	_currentOffset += offset + size;

	return result;
}

DxAllocation DxUploadBuffer::Allocate(uint64 size, uint64 alignment) {
	uint64 alignedOffset = CurrentPage ? AlignTo(CurrentPage->GetOffset(), alignment) : 0;

	DxPage* page = CurrentPage;
	if (not page or page->CheckAvailableSpace(alignedOffset + size)) {
		page = PagePool->GetFreePage();

		if (CurrentPage) {
			PagePool->ReturnPage(CurrentPage);
		}
		CurrentPage = page;
	}

	return page->GetAllocation(size, alignment);
}

void DxUploadBuffer::Reset() {
	if (CurrentPage and PagePool) {
		PagePool->ReturnPage(CurrentPage);
	}
	CurrentPage = nullptr;
}
