#include "DxUploadBuffer.h"
#include "../core/memory.h"

DxPage* DxPagePool::AllocateNewPage() {
	Mutex.Lock();
	auto result = (DxPage*)Arena.Allocate(sizeof(DxPage), true);
	Mutex.Unlock();

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(PageSize);

	auto heapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	ThrowIfFailed(Device->CreateCommittedResource(
		&heapDesc,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(result->Buffer.GetAddressOf())
	));

	result->GpuBasePtr = result->Buffer->GetGPUVirtualAddress();
	result->Buffer->Map(0, nullptr, (void**)&result->CpuBasePtr);
	result->PageSize = PageSize;

	return result;
}

DxPage* DxPagePool::GetFreePage() {
	Mutex.Lock();
	DxPage* result = FreePages;
	if (result) {
		FreePages = result->Next;
	}
	Mutex.Unlock();

	if (not result) {
		result = AllocateNewPage();
	}

	result->CurrentOffset = 0;

	return result;
}

void DxPagePool::ReturnPage(DxPage* page) {
	Mutex.Lock();
	page->Next = UsedPages;
	UsedPages = page;
	if (!LastUsedPage) {
		LastUsedPage = page;
	}
	Mutex.Unlock();
}

void DxPagePool::Reset() {
	if (LastUsedPage) {
		LastUsedPage->Next = FreePages;
	}
	FreePages = UsedPages;
	UsedPages = nullptr;
	LastUsedPage = nullptr;
}

DxAllocation DxUploadBuffer::Allocate(uint64 size, uint64 alignment) {
	assert(size <= PagePool->PageSize);

	uint64 alignedOffset = CurrentPage ? AlignTo(CurrentPage->CurrentOffset, alignment) : 0;

	DxPage* page = CurrentPage;
	if (not page or alignedOffset + size > page->PageSize) {
		page = PagePool->GetFreePage();
		alignedOffset = 0;

		if (CurrentPage) {
			PagePool->ReturnPage(CurrentPage);
		}
		CurrentPage = page;
	}

	DxAllocation result;
	result.CpuPtr = page->CpuBasePtr + alignedOffset;
	result.GpuPtr = page->GpuBasePtr + alignedOffset;

	page->CurrentOffset = alignedOffset + size;

	return result;
}

void DxUploadBuffer::Reset() {
	if (CurrentPage) {
		PagePool->ReturnPage(CurrentPage);
	}
	CurrentPage = nullptr;
}

DxPagePool DxPagePool::Create(DxDevice device, uint64 pageSize) {
	DxPagePool pool = {};
	pool.Device = device;
	pool.Mutex = ThreadMutex::Create();
	pool.PageSize = pageSize;
	return pool;
}