#pragma once

#include "../pch.h"
#include "dx.h"
#include "DxCommandList.h"
#include "DxCommandQueue.h"
#include "../core/threading.h"
#include "../core/memory.h"
#include "DxUploadBuffer.h"
#include "DxDescriptorHeap.h"

struct ObjectRetirement {
	DxObject RetiredObjects[NUM_BUFFERED_FRAMES][128];
	volatile uint32 NumRetireObjects[NUM_BUFFERED_FRAMES];
};

class DxContext {
public:
	static DxContext& Instance() {
		return _instance;
	}

	void Initialize();
	void Quit();

	void NewFrame(uint64 frameId);
	void FlushApplication();

	void RetireObject(DxObject object);

	DxCommandList* GetFreeCopyCommandList();
	DxCommandList* GetFreeComputeCommandList(bool async);
	DxCommandList* GetFreeRenderCommandList();
	uint64 ExecuteCommandList(DxCommandList* commandList);

	DxFactory Factory;
	DxAdapter Adapter;
	DxDevice Device;

	DxCommandQueue RenderQueue;
	DxCommandQueue ComputeQueue;
	DxCommandQueue CopyQueue;

	bool RaytracingSupported;

	uint64 FrameId;
	uint32 BufferedFrameId;

	ThreadMutex AllocationMutex;
	MemoryArena Arena;

	ObjectRetirement ObjectRetirement;

	DxRtvDescriptorHeap RtvAllocator;
	DxDsvDescriptorHeap DsvAllocator;

	DxPagePool PagePools[NUM_BUFFERED_FRAMES];
	DxFrameDescriptorAllocator FrameDescriptorAllocator;

	volatile bool Running = true;

private:
	void CreateFactory();
	void CreateAdapter();
	void CreateDevice();

	DxCommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE type);
	DxCommandList* GetFreeCommandList(DxCommandQueue& queue);
	DxCommandAllocator* GetFreeCommandAllocator(DxCommandQueue& queue);
	DxCommandAllocator* GetFreeCommandAllocator(D3D12_COMMAND_LIST_TYPE type);

	DxCommandList* AllocateCommandList(D3D12_COMMAND_LIST_TYPE type);
	DxCommandAllocator* AllocateCommandAllocator(D3D12_COMMAND_LIST_TYPE type);

	void InitializeBuffer(DxBuffer& buffer, uint32 elementSize, uint32 elementCount, void* data = 0, bool allowUnorderedAccess = false);
	void InitializeDescriptorHeap(DxDescriptorHeap& descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible = true);

	static DxContext& _instance;
};