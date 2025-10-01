#pragma once

#include "../pch.h"
#include "dx.h"

class DxContext;
struct DxDescriptorHandle;

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

	D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress;

	void* Map();
	void Unmap();

	void Upload(DxContext* context, const void* bufferData);
	void UpdateDataRange(DxContext* context, const void* data, uint32 offset, uint32 size);

	static DxBuffer Create(DxContext* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	static DxBuffer CreateUpload(DxContext* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false);
protected:
	void Initialize(DxContext* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT);
	DxDescriptorHandle CreateSRV(DxDevice device, DxDescriptorHandle index, BufferRange bufferRange = {}) const;
	DxDescriptorHandle CreateUAV(DxDevice device, DxDescriptorHandle index, BufferRange bufferRange = {}) const;
	DxDescriptorHandle CreateRawSRV(DxDevice device, DxDescriptorHandle index, BufferRange bufferRange = {});
	DxDescriptorHandle CreateRaytracingAccelerationStructureSRV(DxDevice device, DxDescriptorHandle index) const;
	friend class DxDescriptorRange;
};

class DxVertexBuffer : public DxBuffer {
public:
	D3D12_VERTEX_BUFFER_VIEW View;
	static DxVertexBuffer Create(DxContext* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false);
	static DxVertexBuffer CreateUpload(DxContext* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false);
};

class DxIndexBuffer : public DxBuffer {
public:
	D3D12_INDEX_BUFFER_VIEW View;
	static DxIndexBuffer Create(DxContext* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess);
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