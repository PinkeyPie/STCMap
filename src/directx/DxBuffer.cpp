#include "DxBuffer.h"
#include "DxContext.h"
#include "DxCommandList.h"

void* DxBuffer::Map() {
	void* result;
	Resource->Map(0, nullptr, &result);
	return result;
}

void DxBuffer::Unmap() {
	Resource->Unmap(0, nullptr);
}

void DxBuffer::Upload(const void* bufferData) {
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
	dxContext.RetireObject(intermediateResource);
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

	dxContext.RetireObject(intermediateResource);
	dxContext.ExecuteCommandList(commandList);
}

void DxBuffer::Initialize(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing,
	D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType) {
	DxContext& dxContext = DxContext::Instance();
	D3D12_RESOURCE_FLAGS flags = allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

	ElementSize = elementSize;
	ElementCount = elementCount;
	TotalSize = elementSize * elementCount;
	HeapType = heapType;

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
			Upload(data);
		}
		else if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
			void* dataPtr = Map();
			memcpy(dataPtr, data, TotalSize);
			Unmap();
		}
	}

	DefaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferSRV(this);
	if (allowUnorderedAccess) {
		DefaultUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferUAV(this);
	}
	if (allowClearing) {
		CpuClearUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferSRV(this);
		DxCpuDescriptorHandle shaderVisibleCpuHandle = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateBufferUintUAV(this);
		GpuClearUAV = dxContext.DescriptorAllocatorGPU().GetMatchingGpuHandle(shaderVisibleCpuHandle);
	}
}

DxBuffer DxBuffer::Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing, D3D12_RESOURCE_STATES initialState) {
	DxBuffer result;
	result.Initialize(elementSize, elementCount, data, allowUnorderedAccess, allowClearing, initialState, D3D12_HEAP_TYPE_DEFAULT);
	return result;
}

DxBuffer DxBuffer::CreateUpload(uint32 elementSize, uint32 elementCount, void* data) {
	DxBuffer result;
	result.Initialize(elementSize, elementCount, data, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	return result;
}

DxVertexBuffer DxVertexBuffer::Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing) {
	DxVertexBuffer result;
	result.Initialize(elementSize, elementCount, data, allowUnorderedAccess, allowClearing);
	result.View.BufferLocation = result.GpuVirtualAddress;
	result.View.SizeInBytes = result.TotalSize;
	result.View.StrideInBytes = elementSize;
	return result;
}

DxVertexBuffer DxVertexBuffer::CreateUpload(uint32 elementSize, uint32 elementCount, void* data) {
	DxVertexBuffer result;
	result.Initialize(elementSize, elementCount, data, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	result.View.BufferLocation = result.GpuVirtualAddress;
	result.View.SizeInBytes = result.TotalSize;
	result.View.StrideInBytes = elementSize;
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

DxIndexBuffer DxIndexBuffer::Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing) {
	DxIndexBuffer result;
	result.Initialize(elementSize, elementCount, data, allowUnorderedAccess, allowClearing);
	result.View.BufferLocation = result.GpuVirtualAddress;
	result.View.SizeInBytes = result.TotalSize;
	result.View.Format = GetIndexBufferFormat(elementSize);
	return result;
}

void DxBuffer::Resize(uint32 newElementCount, D3D12_RESOURCE_STATES initialState) {
	DxContext& context = DxContext::Instance();
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

	DefaultSRV.CreateBufferSRV(this);
	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
		DefaultUAV.CreateBufferUAV(this);
	}
	if (CpuClearUAV.CpuHandle.ptr) {
		CpuClearUAV.CreateBufferUintUAV(this);
		DxCpuDescriptorHandle shaderVisibleCPUHandle = context.DescriptorAllocatorCPU().GetMatchingCpuHandle(GpuClearUAV);
		shaderVisibleCPUHandle.CreateBufferUintUAV(this);
	}
}

void DxBuffer::Free(DxBuffer &buffer) {
	DxContext& context = DxContext::Instance();
	if (DefaultSRV.CpuHandle.ptr) {
		context.DescriptorAllocatorCPU().FreeHandle(DefaultSRV);
	}
	if (DefaultUAV.CpuHandle.ptr) {
		context.DescriptorAllocatorCPU().FreeHandle(DefaultUAV);
	}
}

DxSubmesh DxSubmesh::Create(DxMesh& mesh, SubmeshInfo info) {
	DxSubmesh result;
	result.VertexBuffer = mesh.VertexBuffer;
	result.IndexBuffer = mesh.IndexBuffer;
	result.BaseVertex = info.BaseVertex;
	result.FirstTriangle = info.FirstTriangle;
	result.NumTriangles = info.NumTriangles;
	return result;
}

DxSubmesh DxSubmesh::Create(DxMesh& mesh) {
	DxSubmesh result;
	result.VertexBuffer = mesh.VertexBuffer;
	result.IndexBuffer = mesh.IndexBuffer;
	result.BaseVertex = 0;
	result.FirstTriangle = 0;
	result.NumTriangles = mesh.IndexBuffer.ElementCount / 3;
	return result;
}