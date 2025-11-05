#pragma once

#include "../pch.h"
#include "dx.h"
#include "DxDescriptor.h"

class DxContext;
struct DxDescriptorHandle;

struct DxDynamicConstantBuffer {
	D3D12_GPU_VIRTUAL_ADDRESS GpuPtr;
	void* CpuPtr;
};

struct DxDynamicVertexBuffer {
	D3D12_VERTEX_BUFFER_VIEW View;
};

struct BufferRange {
	uint32 FirstElement = 0;
	uint32 NumElements = (uint32)-1;
};

class DxBuffer {
public:
	DxResource Resource;

	uint32 ElementSize;
	uint32 ElementCount;
	uint32 TotalSize;
	D3D12_HEAP_TYPE HeapType;

	D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress;

	DxCpuDescriptorHandle DefaultSRV;
	DxCpuDescriptorHandle DefaultUAV;

	DxCpuDescriptorHandle CpuClearUAV;
	DxGpuDescriptorHandle GpuClearUAV;

	void* Map();
	void Unmap();

	void Upload(const void* bufferData);
	void UpdateDataRange(const void* data, uint32 offset, uint32 size);
	void Resize(uint32 newElementCount, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	void Free(DxBuffer& buffer);

	static DxBuffer Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	static DxBuffer CreateUpload(uint32 elementSize, uint32 elementCount, void* data);

protected:
	void Initialize(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT);

	friend class DxDescriptorRange;
};

class DxVertexBuffer : public DxBuffer {
public:
	D3D12_VERTEX_BUFFER_VIEW View;
	static DxVertexBuffer Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false);
	static DxVertexBuffer CreateUpload(uint32 elementSize, uint32 elementCount, void* data);
};

class DxIndexBuffer : public DxBuffer {
public:
	D3D12_INDEX_BUFFER_VIEW View;
	static DxIndexBuffer Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false);
	static DXGI_FORMAT GetIndexBufferFormat(uint32 elementSize);
};

class DxMesh {
public:
	DxVertexBuffer VertexBuffer;
	DxIndexBuffer IndexBuffer;
};

class SubmeshInfo {
public:
	uint32 NumTriangles;
	uint32 FirstTriangle;
	uint32 BaseVertex;
	uint32 NumVertices;
};

class DxSubmesh {
public:
	DxVertexBuffer VertexBuffer;
	DxIndexBuffer IndexBuffer;
	uint32 NumTriangles;
	uint32 FirstTriangle;
	uint32 BaseVertex;

	static DxSubmesh Create(DxMesh& mesh, SubmeshInfo info);
	static DxSubmesh Create(DxMesh& mesh);
};