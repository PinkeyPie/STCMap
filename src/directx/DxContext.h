#pragma once

#include <mutex>

#include "../pch.h"
#include "dx.h"
#include "DxCommandList.h"
#include "DxCommandQueue.h"
#include "../core/threading.h"
#include "../core/memory.h"
#include "DxUploadBuffer.h"
#include "DxDescriptorHeap.h"

class DxTexture;

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
	bool IsRunning() const { return _running; }

	DxDescriptorHandle PushRenderTargetView(DxTexture* renderTarget) {
		return _rtvAllocator.PushRenderTargetView(renderTarget);
	}
	DxDescriptorHandle PushDepthStencilView(DxTexture* depthBuffer) {
		return _dsvAllocator.PushDepthStencilView(depthBuffer);
	}
	void CreateRenderTargetView(DxTexture* renderTarget, DxDescriptorHandle handle) {
		_rtvAllocator.CreateRenderTargetView(renderTarget, handle);
	}
	void CreateDepthStencilView(DxTexture* depthBuffer, DxDescriptorHandle handle) {
		_dsvAllocator.CreateDepthStencilView(depthBuffer, handle);
	}

	void RetireObject(DxObject object);

	DxCommandList* GetFreeCopyCommandList();
	DxCommandList* GetFreeComputeCommandList(bool async);
	DxCommandList* GetFreeRenderCommandList();
	ID3D12Device5* GetDevice() const { return _device.Get(); }
	IDXGIFactory4* GetFactory() const { return _factory.Get(); }
	IDXGIAdapter4* GetAdapter() const { return _adapter.Get(); }
	DxFrameDescriptorAllocator& FrameDescriptorAllocator() { return _frameDescriptorAllocator; }
	const DxFrameDescriptorAllocator& FrameDescriptorAllocator() const { return _frameDescriptorAllocator; }

	uint64 ExecuteCommandList(DxCommandList* commandList);

	// Carefull with these functions. They are not thread safe
	DxAllocation AllocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	DxDynamicConstantBuffer UploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data);
	template<typename T> DxDynamicConstantBuffer UploadDynamicConstantBuffer(const T& data) {
		return UploadDynamicConstantBuffer(sizeof(T), &data);
	}

	DxCommandQueue RenderQueue = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DxCommandQueue ComputeQueue = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	DxCommandQueue CopyQueue = D3D12_COMMAND_LIST_TYPE_COPY;

	bool CheckTearingSupport();

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

	uint64 _frameId = 0;
	uint32 _bufferFrameId = 0;

	DxFactory _factory = {};
	DxAdapter _adapter = {};
	DxDevice _device = {};

	std::mutex _allocationMutex = {};
	MemoryArena _arena;

	ObjectRetirement _objectRetirement = {};

	DxRtvDescriptorHeap  _rtvAllocator = {};
	DxDsvDescriptorHeap _dsvAllocator = {};

	DxPagePool _pagePools[NUM_BUFFERED_FRAMES] = {{MB(2)}, {MB(2)}};
	DxFrameDescriptorAllocator _frameDescriptorAllocator = {};
	DxUploadBuffer _frameUploadBuffer = {};

	volatile bool _running = true;

	bool _raytracingSupported = false;
	bool _meshShaderSupported = false;
	static DxContext& _instance;
};