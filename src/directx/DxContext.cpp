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
}

void DxContext::CreateFactory() {
	uint32 createFactoryFlags = 0;
#ifdef _DEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(Factory.GetAddressOf())));
}

void DxContext::CreateAdapter() {
	Com<IDXGIAdapter1> dxgiAdapter1;

	size_t maxDedicatedVideoMemory = 0;
	for (uint32 i = 0; Factory->EnumAdapters1(i, dxgiAdapter1.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; i++) {
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
			ThrowIfFailed(dxgiAdapter1.As(&Adapter));
		}
	}
}

void DxContext::CreateDevice() {
	ThrowIfFailed(D3D12CreateDevice(Adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(Device.GetAddressOf())));

#ifdef _DEBUG
	Com<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(Device.As(&infoQueue))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

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

	RaytracingSupported = CheckRaytracingSupport(Device);

	Arena.MinimumBlockSize = MB(2);
	AllocationMutex = ThreadMutex::Create();
	BufferedFrameId = NUM_BUFFERED_FRAMES - 1;

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; i++) {
		PagePools[i] = DxPagePool::Create(Device, MB(2));
	}

	RenderQueue.Initialize(Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	ComputeQueue.Initialize(Device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	CopyQueue.Initialize(Device, D3D12_COMMAND_LIST_TYPE_COPY);

	RtvAllocator = DxRtvDescriptorHeap::CreateRTVDescriptorAllocator(1024);
	DsvAllocator = DxDsvDescriptorHeap::CreateDSVDescriptorAllocator(1024);
	FrameDescriptorAllocator = DxFrameDescriptorAllocator::Create();
}

void DxContext::FlushApplication() {
	RenderQueue.Flush();
	ComputeQueue.Flush();
	CopyQueue.Flush();
}

void DxContext::Quit() {
	Running = false;
	FlushApplication();
	WaitForSingleObject(RenderQueue.ProcessThreadHandle, INFINITE);
	WaitForSingleObject(ComputeQueue.ProcessThreadHandle, INFINITE);
	WaitForSingleObject(CopyQueue.ProcessThreadHandle, INFINITE);
}

DxCommandList* DxContext::AllocateCommandList(D3D12_COMMAND_LIST_TYPE type) {
	AllocationMutex.Lock();
	DxCommandList* result = (DxCommandList*)Arena.Allocate(sizeof(DxCommandList), true);
	AllocationMutex.Unlock();

	result->Type = type;

	DxCommandAllocator* allocator = GetFreeCommandAllocator(type);
	result->CommandAllocator = allocator;

	ThrowIfFailed(Device->CreateCommandList(0, type, allocator->CommandAllocator.Get(), nullptr, IID_PPV_ARGS(result->CommandList.GetAddressOf())));

	return result;
}

DxCommandAllocator* DxContext::AllocateCommandAllocator(D3D12_COMMAND_LIST_TYPE type) {
	AllocationMutex.Lock();
	DxCommandAllocator* result = (DxCommandAllocator*)Arena.Allocate(sizeof(DxCommandAllocator), true);
	AllocationMutex.Unlock();

	ThrowIfFailed(Device->CreateCommandAllocator(type, IID_PPV_ARGS(result->CommandAllocator.GetAddressOf())));

	return result;
}

DxCommandQueue& DxContext::GetQueue(D3D12_COMMAND_LIST_TYPE type) {
	return type == D3D12_COMMAND_LIST_TYPE_DIRECT ? RenderQueue :
		type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? ComputeQueue :
		CopyQueue;
}

DxCommandList* DxContext::GetFreeCommandList(DxCommandQueue& queue) {
	queue.CommandListMutex.Lock();
	DxCommandList* result = queue.FreeCommandLists;
	if (result) {
		queue.FreeCommandLists = result->Next;
	}
	queue.CommandListMutex.Unlock();

	if (!result) {
		result = AllocateCommandList(queue.CommandListType);
	}
	else {
		DxCommandAllocator* allocator = GetFreeCommandAllocator(queue.CommandListType);
		result->Reset(allocator);
	}

	result->UsedLastOnFrame = FrameId;
	result->UploadBuffer.PagePool = &PagePools[BufferedFrameId];

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
	queue.CommandListMutex.Lock();
	DxCommandAllocator* result = queue.FreeCommandAllocators;
	if (result) {
		queue.FreeCommandAllocators = result->Next;
	}
	queue.CommandListMutex.Unlock();

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

	ThrowIfFailed(commandList->CommandList->Close());

	ID3D12CommandList* d3d12List = commandList->CommandList.Get();
	queue.CommandQueue->ExecuteCommandLists(1, &d3d12List);

	uint64 fenceValue = queue.Signal();

	DxCommandAllocator* allocator = commandList->CommandAllocator;
	allocator->LastExecutionFenceValue = fenceValue;

	queue.CommandListMutex.Lock();

	allocator->Next = queue.RunningCommandAllocators;
	queue.RunningCommandAllocators = allocator;
	AtomicIncrement(queue.NumRunningCommandAllocators);

	commandList->Next = queue.FreeCommandLists;
	queue.FreeCommandLists = commandList;

	queue.CommandListMutex.Unlock();

	return fenceValue;
}

void DxContext::RetireObject(DxObject object) {
	if (object) {
		uint32 index = AtomicIncrement(ObjectRetirement.NumRetireObjects[BufferedFrameId]);
		assert(!ObjectRetirement.RetiredObjects[BufferedFrameId][index]);
		ObjectRetirement.RetiredObjects[BufferedFrameId][index] = object;
	}
}

void DxContext::NewFrame(uint64 frameId) {
	FrameId = frameId;

	BufferedFrameId = (uint32)(frameId % NUM_BUFFERED_FRAMES);
	for (uint32 i = 0; i < ObjectRetirement.NumRetireObjects[BufferedFrameId]; i++) {
		ObjectRetirement.RetiredObjects[BufferedFrameId][i].Reset();
	}
	ObjectRetirement.NumRetireObjects[BufferedFrameId] = 0;

	PagePools[BufferedFrameId].Reset();
	FrameDescriptorAllocator.NewFrame(BufferedFrameId);
}

