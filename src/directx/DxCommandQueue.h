#pragma once

#include "../pch.h"
#include "dx.h"
#include "mutex"

class DxCommandList;

class DxCommandQueue {
public:
	const D3D12_COMMAND_LIST_TYPE CommandListType;
	Com<ID3D12CommandQueue> NativeQueue = nullptr;

	DxCommandQueue(const D3D12_COMMAND_LIST_TYPE type) : CommandListType(type) {}

	void Initialize(DxDevice device);

	uint64 TimestampFrequency() const { return _timestampFrequency; }

	uint64 Signal();
	bool IsFenceComplete(uint64 fenceValue) const;
	void WaitForFence(uint64 fenceValue) const;
	void WaitForOtherQueue(DxCommandQueue& other) const;
	void Flush();
	void LeaveThread();

	DxCommandList* GetFreeCommandList();
	uint64 Execute(DxCommandList* commandList);

private:
	void ProcessRunningCommandAllocators();

	uint64 _timestampFrequency; // In hz

	DxCommandList* _runningCommandLists = nullptr;
	DxCommandList* _freeCommandLists = nullptr;

	volatile uint32 _numRunningCommandLists;
	volatile uint32 _totalNumCommandLists;

	std::mutex _commandListMutex = {};
	std::thread _processThread = {};

	Com<ID3D12Fence> _fence = nullptr;
	volatile uint64 _fenceValue = 0;

	friend class DxContext;
};
