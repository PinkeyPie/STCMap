#pragma once

#include "../pch.h"
#include "dx.h"
#include "mutex"

class DxCommandList;
class DxCommandAllocator;

class DxCommandQueue {
public:
	const D3D12_COMMAND_LIST_TYPE CommandListType;
	Com<ID3D12CommandQueue> NativeQueue = nullptr;

	DxCommandQueue(const D3D12_COMMAND_LIST_TYPE type) : CommandListType(type) {}

	void Initialize(DxDevice device);

	uint64 Signal();
	bool IsFenceComplete(uint64 fenceValue) const;
	void WaitForFence(uint64 fenceValue) const;
	void WaitForOtherQueue(DxCommandQueue& other) const;
	void Flush();
	void LeaveThread();
	DxCommandList* GetFreeCommandList();
	DxCommandAllocator* GetFreeCommandAllocator();
	uint64 Execute(DxCommandList* commandList);

private:
	void ProcessRunningCommandAllocators();

	DxCommandAllocator* _runningCommandAllocators = nullptr;
	DxCommandAllocator* _freeCommandAllocators = nullptr;
	volatile uint32 _numRunningCommandAllocators = 0;
	DxCommandList* _freeCommandLists = nullptr;

	std::mutex _mutex = {};
	std::thread _freeAllocThread = {};

	Com<ID3D12Fence> _fence = nullptr;
	volatile uint64 _fenceValue = 0;
};
