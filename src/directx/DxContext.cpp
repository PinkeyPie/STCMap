#include "DxContext.h"

DxContext& DxContext::_instance = *new DxContext{};

namespace {
	void EnableDebugLayer() {
#ifdef _DEBUG
		Com<ID3D12Debug> debugInterface;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugInterface.GetAddressOf())));
		debugInterface->EnableDebugLayer();
#endif
	}

	bool CheckRaytracingSupport(DxDevice device) {
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
		ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
		return options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
	}

	bool CheckMeshShaderSupport(DxDevice device) {
		D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
		ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)));
		return options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
	}
}

void DxContext::CreateFactory() {
	uint32 createFactoryFlags = 0;
#ifdef _DEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(_factory.GetAddressOf())));
}

void DxContext::CreateAdapter() {
	Com<IDXGIAdapter1> dxgiAdapter1;
	DxAdapter dxgiAdapter;

	size_t maxDedicatedVideoMemory = 0;
	for (uint32 i = 0; _factory->EnumAdapters1(i, dxgiAdapter1.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; i++) {
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
		dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

		// Check to see if the adapter can create a D3D12 device without actually 
		// creating it. The adapter with the largest dedicated video memory
		// is favored.
		if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 and 
			SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), 
				D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr)) and
			dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory) {
			maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
			ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter));
		}
	}

	_adapter = dxgiAdapter;
}

void DxContext::CreateDevice() {
	ThrowIfFailed(D3D12CreateDevice(_adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(_device.GetAddressOf())));

#ifdef _DEBUG
// #if 0
	Com<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(_device.As(&infoQueue))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		// infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		// infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		// D3D12_MESSAGE_CATEGORY categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID denyIDs[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER newFilter = {};
		newFilter.DenyList.NumSeverities = std::size(severities);
		newFilter.DenyList.pSeverityList = severities;
		newFilter.DenyList.NumIDs = std::size(denyIDs);
		newFilter.DenyList.pIDList = denyIDs;

		ThrowIfFailed(infoQueue->PushStorageFilter(&newFilter));
	}
#endif
}

void DxContext::Initialize() {
	EnableDebugLayer();

	CreateFactory();
	CreateAdapter();
	CreateDevice();

	_raytracingSupported = CheckRaytracingSupport(_device);
	_meshShaderSupported = CheckMeshShaderSupport(_device);

	_arena.MinimumBlockSize = MB(2);
	_bufferFrameId = NUM_BUFFERED_FRAMES - 1;

	RenderQueue.Initialize(_device);
	ComputeQueue.Initialize(_device);
	CopyQueue.Initialize(_device);

	_descriptorAllocatorCPU.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, false);
	_descriptorAllocatorGPU.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	_rtvAllocator.Initialize(1024, false);
	_dsvAllocator.Initialize(1024, false);
}

void DxContext::FlushApplication() {
	RenderQueue.Flush();
	ComputeQueue.Flush();
	CopyQueue.Flush();
}

void DxContext::Quit() {
	_running = false;
	FlushApplication();
	RenderQueue.LeaveThread();
	ComputeQueue.LeaveThread();
	CopyQueue.LeaveThread();
}

DxCommandList* DxContext::AllocateCommandList(D3D12_COMMAND_LIST_TYPE type) {
	_allocationMutex.lock();
	auto result = static_cast<DxCommandList*>(_arena.Allocate(sizeof(DxCommandList), true));
	_allocationMutex.unlock();

	new(result)DxCommandList();

	result->Type = type;

	DxCommandAllocator* allocator = GetFreeCommandAllocator(type);
	result->_commandAllocator = allocator;

	ThrowIfFailed(_device->CreateCommandList(0, type, allocator->CommandAllocator.Get(), nullptr, IID_PPV_ARGS(result->_commandList.GetAddressOf())))

	result->_dynamicDescriptorHeap.Initialize();

	return result;
}

DxCommandAllocator* DxContext::AllocateCommandAllocator(D3D12_COMMAND_LIST_TYPE type) {
	_allocationMutex.lock();
	auto result = static_cast<DxCommandAllocator*>(_arena.Allocate(sizeof(DxCommandAllocator), true));
	_allocationMutex.unlock();

	ThrowIfFailed(_device->CreateCommandAllocator(type, IID_PPV_ARGS(result->CommandAllocator.GetAddressOf())));

	return result;
}

DxCommandQueue& DxContext::GetQueue(D3D12_COMMAND_LIST_TYPE type) {
	return type == D3D12_COMMAND_LIST_TYPE_DIRECT ? RenderQueue :
		type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? ComputeQueue :
		CopyQueue;
}

DxCommandList* DxContext::GetFreeCommandList(DxCommandQueue& queue) {
	DxCommandList* result = queue.GetFreeCommandList();

	if (!result) {
		result = AllocateCommandList(queue.CommandListType);
	}
	else {
		DxCommandAllocator* allocator = GetFreeCommandAllocator(queue.CommandListType);
		result->Reset(allocator);
	}

	result->_usedLastOnFrame = _frameId;
	result->_uploadBuffer.PagePool = &_pagePools[_bufferFrameId];

	return result;
}

DxCommandList* DxContext::GetFreeCopyCommandList() {
	return GetFreeCommandList(CopyQueue);
}

DxCommandList* DxContext::GetFreeComputeCommandList(bool async) {
	return GetFreeCommandList(async ? ComputeQueue : RenderQueue);
}

DxCommandList* DxContext::GetFreeRenderCommandList() {
	return GetFreeCommandList(RenderQueue);
}

DxCommandAllocator* DxContext::GetFreeCommandAllocator(DxCommandQueue& queue) {
	DxCommandAllocator* result = queue.GetFreeCommandAllocator();

	if (!result) {
		result = AllocateCommandAllocator(queue.CommandListType);
	}
	return result;
}

DxCommandAllocator* DxContext::GetFreeCommandAllocator(D3D12_COMMAND_LIST_TYPE type) {
	DxCommandQueue& queue = GetQueue(type);
	return GetFreeCommandAllocator(queue);
}

uint64 DxContext::ExecuteCommandList(DxCommandList* commandList) {
	DxCommandQueue& queue = GetQueue(commandList->Type);

	return queue.Execute(commandList);
}

DxAllocation DxContext::AllocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment) {
	DxAllocation allocation = _frameUploadBuffer.Allocate(sizeInBytes, alignment);
	return allocation;
}

DxDynamicConstantBuffer DxContext::UploadDynamicConstantBuffer(uint32 sizeInBytes, const void *data) {
	DxAllocation allocation = AllocateDynamicBuffer(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(allocation.CpuPtr, data, sizeInBytes);
	return {allocation.GpuPtr, allocation.CpuPtr};
}

void DxContext::RetireObject(DxObject object) {
	if (object) {
		uint32 index = AtomicIncrement(_objectRetirement.NumRetireObjects[_bufferFrameId]);
		assert(!_objectRetirement.RetiredObjects[_bufferFrameId][index]);
		_objectRetirement.RetiredObjects[_bufferFrameId][index] = object;
	}
}

void DxContext::NewFrame(uint64 frameId) {
	_frameId = frameId;

	_bufferFrameId = (uint32)(frameId % NUM_BUFFERED_FRAMES);
	for (uint32 i = 0; i < _objectRetirement.NumRetireObjects[_bufferFrameId]; i++) {
		_objectRetirement.RetiredObjects[_bufferFrameId][i].Reset();
	}
	_objectRetirement.NumRetireObjects[_bufferFrameId] = 0;

	_frameUploadBuffer.Reset();
	_frameUploadBuffer.PagePool = &_pagePools[_bufferFrameId];

	_pagePools[_bufferFrameId].Reset();
	_frameDescriptorAllocator.NewFrame(_bufferFrameId);
}

bool DxContext::CheckTearingSupport() {
	BOOL allowTearing  = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the
	// graphics debugging tools which will not support the 1.5 factory interface
	// until a future update.
	Com<IDXGIFactory5> factory5;
	if (SUCCEEDED(_factory.As(&factory5))) {
		if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
			allowTearing = FALSE;
		}
	}

	return allowTearing == TRUE;
}

