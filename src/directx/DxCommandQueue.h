#pragma once

#include "dx.h"
#include "../core/threading.h"

class DxCommandList;
class DxCommandAllocator;

class DxCommandQueue {
public:
	void Initialize(DxDevice device, D3D12_COMMAND_LIST_TYPE type);

	uint64 Signal();
	bool IsFenceComplete(uint64 fenceValue);
	void WaitForFence(uint64 fenceValue);
	void WaitForOtherQueue(DxCommandQueue& other);
	void Flush();

	D3D12_COMMAND_LIST_TYPE CommandListType;
	Com<ID3D12CommandQueue> CommandQueue;
	Com<ID3D12Fence> Fence;
	volatile uint64 FenceValue;

	DxCommandAllocator* RunningCommandAllocators;
	DxCommandAllocator* FreeCommandAllocators;
	volatile uint32 NumRunningCommandAllocators;

	DxCommandList* FreeCommandLists;
	HANDLE ProcessThreadHandle;

	ThreadMutex CommandListMutex;
};
