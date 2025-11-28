#include "DxBuffer.h"
#include "DxContext.h"
#include "DxCommandList.h"

namespace {
	void Retire(DxResource resource, DxCpuDescriptorHandle srv, DxCpuDescriptorHandle uav, DxCpuDescriptorHandle clear, DxGpuDescriptorHandle gpuClear, DxCpuDescriptorHandle raytracing) {
		DxContext& context = DxContext::Instance();
		BufferGrave grave;
		grave.Resource = resource;
		grave.Srv = srv;
		grave.Uav = uav;
		grave.Clear = clear;
		grave.Raytracing = raytracing;
		if (gpuClear.GpuHandle.ptr) {
			grave.GpuClear = context.DescriptorAllocatorGPU().GetMatchingCpuHandle(gpuClear);
		}
		else {
			grave.GpuClear = {};
		}
		context.Retire(std::move(grave));
	}
}

void* DxBuffer::Map(bool intentsReading, MapRange readRange) {
	D3D12_RANGE range = { 0, 0 };
	D3D12_RANGE* r = nullptr;

	if (intentsReading) {
		if (readRange.NumElements != -1) {
			range.Begin = readRange.FirstElement * ElementSize;
			range.End = range.Begin + readRange.NumElements * ElementSize;
			r = &range;
		}
	}
	else {
		r = &range;
	}
	void* result;
	Resource->Map(0, r, &result);
	return result;
}

void DxBuffer::Unmap(bool hasWritten, MapRange writtenRange) {
	D3D12_RANGE range = { 0, 0 };
	D3D12_RANGE* r = nullptr;

	if (hasWritten) {
		if (writtenRange.NumElements != -1) {
			range.Begin = writtenRange.FirstElement * ElementSize;
			range.End = range.Begin + writtenRange.NumElements * ElementSize;
		}
	}
	else {
		r = &range;
	}
	Resource->Unmap(0, r);
}

void DxBuffer::UpdateUploadData(void *data, uint32 size) {
	void* mapped = Map(false);
	memcpy(mapped, data, size);
	Unmap(true);
}

void DxBuffer::UploadData(const void* bufferData) {
	DxContext& dxContext = DxContext::Instance();
	DxCommandList* commandList = dxContext.GetFreeCopyCommandList();

	DxResource intermediateResource;

	D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(TotalSize);
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(intermediateResource.GetAddressOf())
	));

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = bufferData;
	subresourceData.RowPitch = TotalSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	commandList->TransitionBarrier(Resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	UpdateSubresources(commandList->CommandList(),
		Resource.Get(), intermediateResource.Get(),
		0, 0, 1, &subresourceData);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue
	dxContext.Retire(intermediateResource);
	dxContext.ExecuteCommandList(commandList);
}

void DxBuffer::UpdateDataRange(const void* data, uint32 offset, uint32 size) {
	assert(offset + size <= TotalSize);

	DxContext& dxContext = DxContext::Instance();
	DxCommandList* commandList = dxContext.GetFreeCopyCommandList();

	DxResource intermediateResource;

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_GPU_UPLOAD);
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(intermediateResource.GetAddressOf())
	));

	commandList->TransitionBarrier(Resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	void* mapped;
	ThrowIfFailed(intermediateResource->Map(0, nullptr, &mapped));
	memcpy(mapped, data, size);
	intermediateResource->Unmap(0, nullptr);

	commandList->CommandList()->CopyBufferRegion(Resource.Get(), offset, intermediateResource.Get(), 0, size);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.

	dxContext.Retire(intermediateResource);
	dxContext.ExecuteCommandList(commandList);
}

void DxBuffer::Initialize(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing,
	bool raytracing, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType) {
	DxContext& dxContext = DxContext::Instance();
	D3D12_RESOURCE_FLAGS flags = allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

	ElementSize = elementSize;
	ElementCount = elementCount;
	TotalSize = elementSize * elementCount;
	HeapType = heapType;
	SupportsSRV = heapType != D3D12_HEAP_TYPE_READBACK;
	SupportsUAV = allowUnorderedAccess;
	SupportsClearing = allowClearing;
	Raytracing = raytracing;

	DefaultSRV = {};
	DefaultUAV = {};
	CpuClearUAV = {};
	GpuClearUAV = {};
	RaytracingSRV = {};

	const auto heapProperties = CD3DX12_HEAP_PROPERTIES(heapType);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(TotalSize, flags);
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(Resource.GetAddressOf())
	));
	GpuVirtualAddress = Resource->GetGPUVirtualAddress();

	if (data) {
		if (heapType == D3D12_HEAP_TYPE_DEFAULT) {
			UploadData(data);
		}
		else if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
			void* dataPtr = Map(false);
			memcpy(dataPtr, data, TotalSize);
			Unmap(true);
		}
	}

	if (SupportsSRV) {
		DefaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferSRV(this);
	}
	if (SupportsUAV) {
		DefaultUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferUAV(this);
	}

	if (SupportsClearing) {
		CpuClearUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferUintUAV(this);
		DxCpuDescriptorHandle shaderVisibleCpuHandle = dxContext.DescriptorAllocatorGPU().GetFreeHandle().CreateBufferUintUAV(this);
		GpuClearUAV = dxContext.DescriptorAllocatorGPU().GetMatchingGpuHandle(shaderVisibleCpuHandle);
	}

	if (raytracing) {
		RaytracingSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateRaytracingAccelerationStructureSRV(this);
	}
}

Ptr<DxBuffer> DxBuffer::Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing, D3D12_RESOURCE_STATES initialState) {
	Ptr<DxBuffer> result = MakePtr<DxBuffer>();
	result->Initialize(elementSize, elementCount, data, allowUnorderedAccess, allowClearing, false, initialState, D3D12_HEAP_TYPE_DEFAULT);
	return result;
}

Ptr<DxBuffer> DxBuffer::CreateUpload(uint32 elementSize, uint32 elementCount, void* data) {
	Ptr<DxBuffer> result = MakePtr<DxBuffer>();
	result->Initialize(elementSize, elementCount, data, false, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	return result;
}

Ptr<DxBuffer> DxBuffer::CreateReadback(uint32 elementSize, uint32 elementCount, D3D12_RESOURCE_STATES initialState) {
	Ptr<DxBuffer> result = MakePtr<DxBuffer>();
	result->Initialize(elementSize, elementCount, nullptr, false, false, false, initialState, D3D12_HEAP_TYPE_READBACK);
	return result;
}

Ptr<DxVertexBuffer> DxVertexBuffer::Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing) {
	Ptr<DxVertexBuffer> result = MakePtr<DxVertexBuffer>();
	result->Initialize(elementSize, elementCount, data, allowUnorderedAccess, allowClearing);
	result->View.BufferLocation = result->GpuVirtualAddress;
	result->View.SizeInBytes = result->TotalSize;
	result->View.StrideInBytes = elementSize;
	return result;
}

Ptr<DxVertexBuffer> DxVertexBuffer::CreateUpload(uint32 elementSize, uint32 elementCount, void* data) {
	Ptr<DxVertexBuffer> result = MakePtr<DxVertexBuffer>();
	result->Initialize(elementSize, elementCount, data, false, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	result->View.BufferLocation = result->GpuVirtualAddress;
	result->View.SizeInBytes = result->TotalSize;
	result->View.StrideInBytes = elementSize;
	return result;
}

DXGI_FORMAT DxIndexBuffer::GetIndexBufferFormat(uint32 elementSize) {
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
	if (elementSize == 1) {
		result = DXGI_FORMAT_R8_UINT;
	}
	else if (elementSize == 2) {
		result = DXGI_FORMAT_R16_UINT;
	}
	else if (elementSize == 4) {
		result = DXGI_FORMAT_R32_UINT;
	}
	return result;
}

Ptr<DxIndexBuffer> DxIndexBuffer::Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing) {
	Ptr<DxIndexBuffer> result = MakePtr<DxIndexBuffer>();
	result->Initialize(elementSize, elementCount, data, allowUnorderedAccess, allowClearing);
	result->View.BufferLocation = result->GpuVirtualAddress;
	result->View.SizeInBytes = result->TotalSize;
	result->View.Format = GetIndexBufferFormat(elementSize);
	return result;
}

Ptr<DxBuffer> DxBuffer::CreateRaytracingTLASBuffer(uint32 size) {
	Ptr<DxBuffer> result = MakePtr<DxBuffer>();
	result->Initialize(size, 1, nullptr, true, false, true, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	return result;
}

DxBuffer::~DxBuffer() {
	wchar name[128];

	if (Resource) {
		uint32 size = sizeof(name);
		Resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, name);
		name[min(arraysize(name) - 1, size)] = 0;
	}

	Retire(Resource, DefaultSRV, DefaultUAV, CpuClearUAV, GpuClearUAV, RaytracingSRV);
}

void DxBuffer::Resize(uint32 newElementCount, D3D12_RESOURCE_STATES initialState) {
	DxContext& context = DxContext::Instance();

	Retire(Resource, DefaultSRV, DefaultUAV, CpuClearUAV, GpuClearUAV, RaytracingSRV);

	ElementCount = newElementCount;
	TotalSize = ElementCount * ElementSize;

	auto desc = Resource->GetDesc();

	CD3DX12_HEAP_PROPERTIES heapProperties(HeapType);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(TotalSize, desc.Flags);
	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(Resource.GetAddressOf())
	));
	GpuVirtualAddress = Resource->GetGPUVirtualAddress();

	if (SupportsSRV) {
		DefaultSRV = context.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferSRV(this);
	}
	if (SupportsUAV) {
		DefaultUAV = context.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferUAV(this);
	}
	if (SupportsClearing) {
		CpuClearUAV = context.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferUintUAV(this);
		DxCpuDescriptorHandle shaderVisibleCPUHandle = context.DescriptorAllocatorGPU().GetFreeHandle().CreateBufferUintUAV(this);
		GpuClearUAV = context.DescriptorAllocatorGPU().GetMatchingGpuHandle(shaderVisibleCPUHandle);
	}

	if (Raytracing) {
		RaytracingSRV = context.DescriptorAllocatorCPU().GetFreeHandle().CreateRaytracingAccelerationStructureSRV(this);
	}
}

BufferGrave::~BufferGrave() {
	DxContext& context = DxContext::Instance();

	if (Resource) {
		if (Srv.CpuHandle.ptr) {
			context.DescriptorAllocatorCPU().FreeHandle(Srv);
		}
		if (Uav.CpuHandle.ptr) {
			context.DescriptorAllocatorCPU().FreeHandle(Uav);
		}
		if (Clear.CpuHandle.ptr) {
			context.DescriptorAllocatorCPU().FreeHandle(Clear);
			context.DescriptorAllocatorGPU().FreeHandle(GpuClear);
		}
		if (Raytracing.CpuHandle.ptr) {
			context.DescriptorAllocatorCPU().FreeHandle(Raytracing);
		}
	}
}