#include "DxCommandQueue.h"

#include "DxContext.h"

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

	_freeAllocThread = std::thread([&] { ProcessRunningCommandAllocators(); });

	switch (CommandListType) {
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		SetName(NativeQueue, "Render command queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		SetName(NativeQueue, "Compute command queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		SetName(NativeQueue, "Copy command queue");
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
	while (_numRunningCommandAllocators) {}

	WaitForFence(Signal());
}

void DxCommandQueue::LeaveThread() {
	if (_freeAllocThread.joinable()) {
		_freeAllocThread.join();
	}
}

void DxCommandQueue::ProcessRunningCommandAllocators() {
	DxContext& dxContext = DxContext::Instance();
	while (dxContext.IsRunning()) {
		while (true) {
			_mutex.lock();
			DxCommandAllocator* allocator = _runningCommandAllocators;
			if (allocator) {
				_runningCommandAllocators = allocator->Next;
			}
			_mutex.unlock();

			if (allocator) {
				WaitForFence(allocator->LastExecutionFenceValue);
				allocator->CommandAllocator->Reset();

				_mutex.lock();
				allocator->Next = _freeCommandAllocators;
				_freeCommandAllocators = allocator;
				AtomicDecrement(_numRunningCommandAllocators);
				_mutex.unlock();
			}
			else {
				break;
			}
		}

		std::this_thread::yield();
	}
}

DxCommandList *DxCommandQueue::GetFreeCommandList() {
	_mutex.lock();
	DxCommandList* result = _freeCommandLists;
	if (result) {
		_freeCommandLists = result->Next;
	}
	_mutex.unlock();

	return result;
}

DxCommandAllocator *DxCommandQueue::GetFreeCommandAllocator() {
	_mutex.lock();
	DxCommandAllocator* result = _freeCommandAllocators;
	if (result) {
		_freeCommandAllocators = result->Next;
	}
	_mutex.unlock();

	return result;
}

uint64 DxCommandQueue::Execute(DxCommandList* commandList) {
	ThrowIfFailed(commandList->CommandList->Close());

	ID3D12CommandList* d3d12List = commandList->CommandList.Get();
	NativeQueue->ExecuteCommandLists(1, &d3d12List);

	uint64 fenceValue = Signal();

	DxCommandAllocator* allocator = commandList->CommandAllocator;
	allocator->LastExecutionFenceValue = fenceValue;

	_mutex.lock();

	allocator->Next = _runningCommandAllocators;
	_runningCommandAllocators = allocator;
	AtomicIncrement(_numRunningCommandAllocators);

	commandList->Next = _freeCommandLists;
	_freeCommandLists = commandList;

	_mutex.unlock();

	return _fenceValue;
}
