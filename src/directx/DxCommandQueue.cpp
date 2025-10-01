#include "DxCommandQueue.h"

#include "DxContext.h"

namespace {
	DWORD ProcessRunningCommandAllocators(void* data);
}

void DxCommandQueue::Initialize(DxDevice device, D3D12_COMMAND_LIST_TYPE type) {
	FenceValue = 0;
	CommandListType = type;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(CommandQueue.GetAddressOf())));
	ThrowIfFailed(device->CreateFence(FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.GetAddressOf())));

	CommandListMutex = ThreadMutex::Create();
	ProcessThreadHandle = CreateThread(nullptr, 0, ProcessRunningCommandAllocators, this, 0, nullptr);

	switch (type) {
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		SetName(CommandQueue, "Render command queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		SetName(CommandQueue, "Compute command queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		SetName(CommandQueue, "Copy command queue");
		break;
	}
}

uint64 DxCommandQueue::Signal() {
	uint64 fenceValueForSignal = AtomicIncrement(FenceValue) + 1;
	ThrowIfFailed(CommandQueue->Signal(Fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

bool DxCommandQueue::IsFenceComplete(uint64 fenceValue) {
	return Fence->GetCompletedValue() >= fenceValue;
}

void DxCommandQueue::WaitForFence(uint64 fenceValue) {
	if (!IsFenceComplete(fenceValue)) {
		HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(fenceEvent, DWORD_MAX);

		Fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, DWORD_MAX);

		CloseHandle(fenceEvent);
	}
}

void DxCommandQueue::WaitForOtherQueue(DxCommandQueue& other) {
	CommandQueue->Wait(other.Fence.Get(), other.Signal());
}

void DxCommandQueue::Flush() {
	while (NumRunningCommandAllocators) {}

	WaitForFence(Signal());
}

namespace {
	DWORD ProcessRunningCommandAllocators(void* data) {
		DxCommandQueue& queue = *(DxCommandQueue*)data;

		while (DxContext::Instance().Running) {
			while (true) {
				queue.CommandListMutex.Lock();
				DxCommandAllocator* allocator = queue.RunningCommandAllocators;
				if (allocator) {
					queue.RunningCommandAllocators = allocator->Next;
				}
				queue.CommandListMutex.Unlock();

				if (allocator) {
					queue.WaitForFence(allocator->LastExecutionFenceValue);
					allocator->CommandAllocator.Reset();

					queue.CommandListMutex.Lock();
					allocator->Next = queue.FreeCommandAllocators;
					queue.FreeCommandAllocators = allocator;
					AtomicDecrement(queue.NumRunningCommandAllocators);
					queue.CommandListMutex.Unlock();
				}
				else {
					break;
				}
			}

			SwitchToThread(); // Yield
		}

		return 0;
	}
}

