#include "DxContext.h"

#include <iostream>

#include "DxProfiling.h"
#include "../core/threading.h"

extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

DxContext& DxContext::_instance = *new DxContext{};

#if ENABLE_DX_PROFILING
// Defined in dx_profiling.cpp.
void ProfileFrameMarker(DxCommandList* cl);
void ResolveTimeStampQueries(uint64* timestamps);
#endif

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
		if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)))) {
			return options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
		}
		return false;
	}

	bool CheckMeshShaderSupport(DxDevice device) {
#if ADVANCED_GPU_FEATURES_ENABLED
		D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
		if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)))) {
			return options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
		}
		else {
			std::cerr << "Checking support for mesh shader feature failed. Maybe you need to update your Windows version." << std::endl;
		}
#endif
		return false;
	}
}

void DxContext::CreateFactory() {
	uint32 createFactoryFlags = 0;
#ifdef _DEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(_factory.GetAddressOf())));
}

D3D_FEATURE_LEVEL DxContext::CreateAdapter(D3D_FEATURE_LEVEL minimumFeatureLevel) {
	Com<IDXGIAdapter1> dxgiAdapter1;
	DxAdapter dxgiAdapter;
	DXGI_ADAPTER_DESC1 desc;

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_9_1;

	D3D_FEATURE_LEVEL possibleFeatureLevels[] = {
		D3D_FEATURE_LEVEL_9_1,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_12_1
	};

	uint32 firstFeatureLevel = 0;
	for (uint32 i = 0; i < std::size(possibleFeatureLevels); i++) {
		if (possibleFeatureLevels[i] == minimumFeatureLevel) {
			firstFeatureLevel = i;
			break;
		}
	}

	uint64 maxDedicatedVideoMemory = 0;
	for (uint32 i = 0; _factory->EnumAdapters1(i, dxgiAdapter1.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; i++) {
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
		dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

		// Check to see if the adapter can create a D3D12 device without actually 
		// creating it. The adapter with the largest dedicated video memory
		// is favored.
		if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
			D3D_FEATURE_LEVEL adapterFeatureLevel = D3D_FEATURE_LEVEL_9_1;
			bool supportsFeatureLevel = false;

			for (uint32 fl = firstFeatureLevel; fl < std::size(possibleFeatureLevels); fl++) {
				if (SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), possibleFeatureLevels[fl], __uuidof(ID3D12Device), nullptr))) {
					adapterFeatureLevel = possibleFeatureLevels[fl];
					supportsFeatureLevel = true;
				}
			}

			if (supportsFeatureLevel and dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory) {
				ThrowIfFailed(dxgiAdapter1.As(&_adapter));
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				featureLevel = adapterFeatureLevel;
				desc = dxgiAdapterDesc1;
			}
		}
	}

	printf("Using GPU: %ls\n", desc.Description);

	return featureLevel;
}

void DxContext::CreateDevice(D3D_FEATURE_LEVEL featureLevel) {
	ThrowIfFailed(D3D12CreateDevice(_adapter.Get(), featureLevel, IID_PPV_ARGS(_device.GetAddressOf())));

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

bool DxContext::Initialize() {
	EnableDebugLayer();

	CreateFactory();
	D3D_FEATURE_LEVEL adapterFl = CreateAdapter();
	if (not _adapter) {
		std::cerr << "No DX12 capable GPU found." << std::endl;
		return false;
	}

	CreateDevice(adapterFl);
	_raytracingSupported = CheckRaytracingSupport(_device);
	_meshShaderSupported = CheckMeshShaderSupport(_device);

	_bufferFrameId = NUM_BUFFERED_FRAMES - 1;

	_descriptorHandleIncrementSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_descriptorAllocatorCPU.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, false);
	_descriptorAllocatorGPU.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	_rtvAllocator.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, false);
	_dsvAllocator.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024, false);

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; i++) {
#if ENABLE_DX_PROFILING
		_timestampHeaps[i].Initialize(MAX_NUM_DX_PROFILE_EVENTS);
		_resolvedTimestampBuffers[i] = DxBuffer::CreateReadback(sizeof(uint64), MAX_NUM_DX_PROFILE_EVENTS);
#endif
	}

	_frameUploadBuffer.Reset();
	_frameUploadBuffer.PagePool = &_pagePools[_bufferFrameId];

	_pagePools[_bufferFrameId].Reset();

	RenderQueue.Initialize(_device);
	ComputeQueue.Initialize(_device);
	CopyQueue.Initialize(_device);

	return true;
}

void DxContext::FlushApplication() {
	RenderQueue.Flush();
	ComputeQueue.Flush();
	CopyQueue.Flush();
}

void DxContext::Quit() {
#if ENABLE_DX_PROFILING
	for (uint32 b = 0; b < NUM_BUFFERED_FRAMES; b++) {

	}
#endif

	_running = false;
	FlushApplication();
	RenderQueue.LeaveThread();
	ComputeQueue.LeaveThread();
	CopyQueue.LeaveThread();

	for (uint32 b = 0; b < NUM_BUFFERED_FRAMES; b++) {
		_textureGraveyard[b].clear();
		_bufferedGraveyard[b].clear();
		_objectGraveyard[b].clear();
	}
}

void DxContext::Retire(TextureGrave &&texture) {
	_mutex.lock();
	_textureGraveyard[_bufferFrameId].push_back(std::move(texture));
	_mutex.unlock();
}

void DxContext::Retire(struct BufferGrave &&buffer) {
	_mutex.lock();
	_bufferedGraveyard[_bufferFrameId].push_back(std::move(buffer));
	_mutex.unlock();
}

void DxContext::Retire(DxObject obj) {
	_mutex.lock();
	_objectGraveyard[_bufferFrameId].push_back(obj);
	_mutex.unlock();
}
DxCommandQueue& DxContext::GetQueue(D3D12_COMMAND_LIST_TYPE type) {
	return type == D3D12_COMMAND_LIST_TYPE_DIRECT ? RenderQueue :
		type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? ComputeQueue :
		CopyQueue;
}

DxCommandList* DxContext::GetFreeCommandList(DxCommandQueue& queue) {
	DxCommandList* result = queue.GetFreeCommandList();

	_mutex.lock();
	result->_uploadBuffer.PagePool = &_pagePools[_bufferFrameId];
	_mutex.unlock();

#if ENABLE_DX_PROFILING
	result->_timeStampQueryHeap = _timestampHeaps[_bufferFrameId].Heap;
#endif

	return result;
}

DxCommandList* DxContext::GetFreeCopyCommandList() {
	return GetFreeCommandList(CopyQueue);
}

DxCommandList* DxContext::GetFreeComputeCommandList(bool async) {
	return GetFreeCommandList(async ? ComputeQueue : RenderQueue);
}

DxCommandList* DxContext::GetFreeRenderCommandList() {
	DxCommandList* commandList = GetFreeCommandList(RenderQueue);
	CD3DX12_RECT scissorRect(0, 0, LONG_MAX, LONG_MAX);
	commandList->SetScissor(scissorRect);
	return commandList;
}

uint64 DxContext::ExecuteCommandList(DxCommandList* commandList) {
	DxCommandQueue& queue = GetQueue(commandList->Type());
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

DxMemoryUsage DxContext::GetMemoryUsage() {
	DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
	ThrowIfFailed(_instance._adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo));

	return { (uint32)BYTE_TO_MB(memoryInfo.CurrentUsage), (uint32)BYTE_TO_MB(memoryInfo.Budget) };
}

void DxContext::EndFrame(DxCommandList *cl) {
#if ENABLE_DX_PROFILING
	ProfileFrameMarker(cl);
	cl->CommandList()->ResolveQueryData(_timestampHeaps[_bufferFrameId].Heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, _timestampQueryIndex[_bufferFrameId], _resolvedTimestampBuffers[_bufferFrameId]->Resource.Get(), 0);
#endif
}

void DxContext::NewFrame(uint64 frameId) {
	_frameId = frameId;

	_mutex.lock();
	_bufferFrameId = (uint32)(frameId % NUM_BUFFERED_FRAMES);

#if ENABLE_DX_PROFILING
	uint64* timestamps = (uint64*)_resolvedTimestampBuffers[_bufferFrameId]->Map(true);
	ResolveTimeStampQueries(timestamps);
	_resolvedTimestampBuffers[_bufferFrameId]->Unmap(false);

	_timestampQueryIndex[_bufferFrameId] = 0;
#endif

	_textureGraveyard[_bufferFrameId].clear();
	_bufferedGraveyard[_bufferFrameId].clear();
	_objectGraveyard[_bufferFrameId].clear();

	_frameUploadBuffer.Reset();
	_frameUploadBuffer.PagePool = &_pagePools[_bufferFrameId];

	_pagePools[_bufferFrameId].Reset();
	_frameDescriptorAllocator.NewFrame(_bufferFrameId);

	_mutex.unlock();
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

