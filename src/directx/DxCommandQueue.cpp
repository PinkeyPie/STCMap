#include "DxCommandQueue.h"

#include "DxContext.h"
#include "../core/threading.h"
#include "DxCommandList.h"

namespace {
	DWORD ProcessRunningCommandAllocators(void* data);
}

void DxCommandQueue::Initialize(DxDevice device) {
	_fenceValue = 0;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = CommandListType;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(NativeQueue.GetAddressOf())));
	ThrowIfFailed(device->CreateFence(_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.GetAddressOf())));

	_timestampFrequency = 0; // Default value, if timing is not supported on this queue
	if (SUCCEEDED(NativeQueue->GetTimestampFrequency(&_timestampFrequency))) {
		// TODO: Calibrate command queue time line with CPU.
	}

	_processThread = std::thread([&] { ProcessRunningCommandAllocators(); });

	switch (CommandListType) {
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		SET_NAME(NativeQueue, "Render command queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		SET_NAME(NativeQueue, "Compute command queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		SET_NAME(NativeQueue, "Copy command queue");
		break;
	default:
		break;
	}
}

uint64 DxCommandQueue::Signal() {
	uint64 fenceValueForSignal = AtomicIncrement(_fenceValue) + 1;
	ThrowIfFailed(NativeQueue->Signal(_fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

bool DxCommandQueue::IsFenceComplete(const uint64 fenceValue) const {
	return _fence->GetCompletedValue() >= fenceValue;
}

void DxCommandQueue::WaitForFence(const uint64 fenceValue) const {
	if (not IsFenceComplete(fenceValue)) {
		HANDLE fenceEvent = CreateEventExW(nullptr, L"", 0, EVENT_ALL_ACCESS);
		assert(fenceEvent, DWORD_MAX);

		// Fire event when GPU hits current fence
		ThrowIfFailed(_fence->SetEventOnCompletion(fenceValue, fenceEvent));

		// Wait until GPU ends event
		WaitForSingleObject(fenceEvent, DWORD_MAX);
		CloseHandle(fenceEvent);
	}
}

void DxCommandQueue::WaitForOtherQueue(DxCommandQueue& other) const {
	ThrowIfFailed(NativeQueue->Wait(other._fence.Get(), other.Signal()));
}

void DxCommandQueue::Flush() {
	while (_numRunningCommandLists) {}

	WaitForFence(Signal());
}

void DxCommandQueue::LeaveThread() {
	if (_processThread.joinable()) {
		_processThread.join();
	}
}

void DxCommandQueue::ProcessRunningCommandAllocators() {
	DxContext& dxContext = DxContext::Instance();

	while (dxContext.IsRunning()) {
		while (true) {
			_commandListMutex.lock();
			DxCommandList* list = _runningCommandLists;
			if (list) {
				_runningCommandLists = list->_next;
			}
			_commandListMutex.unlock();

			if (list) {
				WaitForFence(list->_lastExecutionFenceValue);
				list->Reset();

				_commandListMutex.lock();
				list->_next = _freeCommandLists;
				_freeCommandLists = list;
				AtomicDecrement(_numRunningCommandLists);
				_commandListMutex.unlock();
			}
			else {
				break;
			}
		}

		std::this_thread::yield();
	}
}

DxCommandList *DxCommandQueue::GetFreeCommandList() {
	_commandListMutex.lock();
	DxCommandList* result = _freeCommandLists;
	if (result) {
		_freeCommandLists = result->_next;
	}
	_commandListMutex.unlock();

	if (not result) {
		result = new DxCommandList(CommandListType);
		ThrowIfFailed(result->_commandList->Close());
		result->Reset();
		AtomicIncrement(_totalNumCommandLists);
	}


	return result;
}

uint64 DxCommandQueue::Execute(DxCommandList* commandList) {
	ThrowIfFailed(commandList->CommandList()->Close());

	ID3D12CommandList* d3d12List = commandList->CommandList();
	NativeQueue->ExecuteCommandLists(1, &d3d12List);

	uint64 fenceValue = Signal();

	commandList->_lastExecutionFenceValue = fenceValue;

	_commandListMutex.lock();

	commandList->_next = _runningCommandLists;
	_runningCommandLists = commandList;
	AtomicIncrement(_numRunningCommandLists);

	_commandListMutex.unlock();

	return _fenceValue;
}
