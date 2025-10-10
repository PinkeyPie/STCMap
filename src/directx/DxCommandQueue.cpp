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

	ProcessThread = std::thread([&]() {
		ProcessRunningCommandAllocators();
	});

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
	default:
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
	if (not IsFenceComplete(fenceValue)) {
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

void DxCommandQueue::ProcessRunningCommandAllocators() {
	DxContext& dxContext = DxContext::Instance();
	while (dxContext.Running) {
		while (true) {
			CommandListMutex.lock();
			DxCommandAllocator* allocator = RunningCommandAllocators;
			if (allocator) {
				RunningCommandAllocators = allocator->Next;
			}
			CommandListMutex.unlock();

			if (allocator) {
				WaitForFence(allocator->LastExecutionFenceValue);
				allocator->CommandAllocator.Reset();

				CommandListMutex.lock();
				allocator->Next = FreeCommandAllocators;
				FreeCommandAllocators = allocator;
				AtomicDecrement(NumRunningCommandAllocators);
				CommandListMutex.unlock();
			}
			else {
				break;
			}
		}

		std::this_thread::yield();
	}
}
