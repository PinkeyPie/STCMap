#pragma once

#include "dx.h"
#include "DxCommandQueue.h"
#include "../core/threading.h"
#include "../core/memory.h"
#include "DxUploadBuffer.h"
#include "DxDescriptorAllocation.h"
#include "DxBuffer.h"
#include "DxQuery.h"


struct DxMemoryUsage {
	// In MB
	uint32 CurrentlyUsed;
	uint32 Available;
};

class DxContext {
public:
	static DxContext& Instance() {
		return _instance;
	}

	bool Initialize();
	void Quit();

	void NewFrame(uint64 frameId);
	void EndFrame(DxCommandList* cl);
	void FlushApplication();

	DxCommandList* GetFreeCopyCommandList();
	DxCommandList* GetFreeComputeCommandList(bool async);
	DxCommandList* GetFreeRenderCommandList();
	uint64 ExecuteCommandList(DxCommandList* commandList);

	// Carefully with these functions. They are not thread safe
	DxAllocation AllocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	DxDynamicConstantBuffer UploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data);
	template<typename T> DxDynamicConstantBuffer UploadDynamicConstantBuffer(const T& data) {
		return UploadDynamicConstantBuffer(sizeof(T), &data);
	}

	static DxMemoryUsage GetMemoryUsage();

	ID3D12Device5* GetDevice() const { return _device.Get(); }
	IDXGIFactory4* GetFactory() const { return _factory.Get(); }
	IDXGIAdapter4* GetAdapter() const { return _adapter.Get(); }

	DxCommandQueue RenderQueue = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DxCommandQueue ComputeQueue = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	DxCommandQueue CopyQueue = D3D12_COMMAND_LIST_TYPE_COPY;

	bool CheckTearingSupport();
	bool RaytracingSupported() const { return false; }
	bool MeshShaderSupported() const { return false; }

	uint64 DescriptorHandleIncrementSize() const { return _descriptorHandleIncrementSize; }
	uint64 FrameId() const { return _frameId; }
	uint64 BufferedFrameId() const { return _bufferFrameId; }

	const DxDescriptorHeap<DxCpuDescriptorHandle>& DescriptorAllocatorCPU() const { return _descriptorAllocatorCPU; }
	DxDescriptorHeap<DxCpuDescriptorHandle>& DescriptorAllocatorCPU() { return _descriptorAllocatorCPU; }
	const DxDescriptorHeap<DxCpuDescriptorHandle>& DescriptorAllocatorGPU() const { return _descriptorAllocatorGPU; }
	DxDescriptorHeap<DxCpuDescriptorHandle>& DescriptorAllocatorGPU() { return _descriptorAllocatorGPU; }

	const DxDescriptorHeap<DxRtvDescriptorHandle>& RtvAllocator() const { return _rtvAllocator; }
	DxDescriptorHeap<DxRtvDescriptorHandle>& RtvAllocator() { return _rtvAllocator; }
	const DxDescriptorHeap<DxDsvDescriptorHandle>& DsvAllocator() const { return _dsvAllocator; }
	DxDescriptorHeap<DxDsvDescriptorHandle>& DsvAllocator() { return _dsvAllocator; }

	bool IsRunning() const { return _running; }

	void Retire(struct TextureGrave&& texture);
	void Retire(struct BufferGrave&& buffer);
	void Retire(DxObject obj);


	DxFrameDescriptorAllocator& FrameDescriptorAllocator() { return _frameDescriptorAllocator; }
	const DxFrameDescriptorAllocator& FrameDescriptorAllocator() const { return _frameDescriptorAllocator; }
#if ENABLE_DX_PROFILING
	uint32 TimestampQueryIndex() const { return _timestampQueryIndex[_bufferFrameId]; }
	uint32 IncrementQueryIndex() { return AtomicIncrement(_timestampQueryIndex[_bufferFrameId]); }
#endif

private:
	void CreateFactory();
	D3D_FEATURE_LEVEL CreateAdapter(D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_11_0);
	void CreateDevice(D3D_FEATURE_LEVEL featureLevel);

	DxCommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE type);
	DxCommandList* GetFreeCommandList(DxCommandQueue& queue);

	uint64 _frameId = 0;
	uint32 _bufferFrameId = 0;

	DxFactory _factory = {};
	DxAdapter _adapter = {};
	DxDevice _device = {};

	DxDescriptorHeap<DxCpuDescriptorHandle> _descriptorAllocatorCPU;
	DxDescriptorHeap<DxCpuDescriptorHandle> _descriptorAllocatorGPU; // Used for resources, which can be UAV cleared.

	DxDescriptorHeap<DxRtvDescriptorHandle> _rtvAllocator;
	DxDescriptorHeap<DxDsvDescriptorHandle> _dsvAllocator;

#if ENABLE_DX_PROFILING
	uint32 _timestampQueryIndex[NUM_BUFFERED_FRAMES] = {};
	DxTimestampQueryHeap _timestampHeaps[NUM_BUFFERED_FRAMES];
	Ptr<DxBuffer> _resolvedTimestampBuffers[NUM_BUFFERED_FRAMES];
#endif

	std::mutex _mutex = {};
	MemoryArena _arena;

	DxPagePool _pagePools[NUM_BUFFERED_FRAMES] = {{MB(2)}, {MB(2)}};
	DxFrameDescriptorAllocator _frameDescriptorAllocator = {};
	DxUploadBuffer _frameUploadBuffer = {};

	uint32 _descriptorHandleIncrementSize = 0;

	volatile bool _running = true;

	std::vector<struct TextureGrave> _textureGraveyard[NUM_BUFFERED_FRAMES];
	std::vector<struct BufferGrave> _bufferedGraveyard[NUM_BUFFERED_FRAMES];
	std::vector<DxObject> _objectGraveyard[NUM_BUFFERED_FRAMES];

	bool _raytracingSupported = false;
	bool _meshShaderSupported = false;
	static DxContext& _instance;
};