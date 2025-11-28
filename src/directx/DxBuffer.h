#pragma once

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

struct MapRange {
	uint32 FirstElement = 0;
	uint32 NumElements = (uint32)-1;
};

class DxBuffer {
public:
	virtual ~DxBuffer();

	DxResource Resource = nullptr;

	uint32 ElementSize = 0;
	uint32 ElementCount = 0;
	uint32 TotalSize = 0;
	D3D12_HEAP_TYPE HeapType = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress;

	bool SupportsUAV;
	bool SupportsSRV;
	bool SupportsClearing;
	bool Raytracing;

	DxCpuDescriptorHandle DefaultSRV;
	DxCpuDescriptorHandle DefaultUAV;

	DxCpuDescriptorHandle CpuClearUAV;
	DxGpuDescriptorHandle GpuClearUAV;

	DxCpuDescriptorHandle RaytracingSRV;

	void* Map(bool intentsReading, MapRange readRange = {});
	void Unmap(bool hasWritten, MapRange writenRange = {});

	void UploadData(const void* bufferData);
	void UpdateDataRange(const void* data, uint32 offset, uint32 size);
	void UpdateUploadData(void* data, uint32 size);
	void Resize(uint32 newElementCount, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

	static Ptr<DxBuffer> Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	static Ptr<DxBuffer> CreateUpload(uint32 elementSize, uint32 elementCount, void* data);
	static Ptr<DxBuffer> CreateRaytracingTLASBuffer(uint32 size);
	static Ptr<DxBuffer> CreateReadback(uint32 elementSize, uint32 elementCount, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COPY_DEST);

protected:
	void Initialize(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing, bool raytracing = false,
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT);

	friend class DxDescriptorRange;
};

class DxVertexBuffer : public DxBuffer {
public:
	D3D12_VERTEX_BUFFER_VIEW View;
	static Ptr<DxVertexBuffer> Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false);
	static Ptr<DxVertexBuffer> CreateUpload(uint32 elementSize, uint32 elementCount, void* data);
};

class DxIndexBuffer : public DxBuffer {
public:
	D3D12_INDEX_BUFFER_VIEW View;
	static Ptr<DxIndexBuffer> Create(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false);
	static DXGI_FORMAT GetIndexBufferFormat(uint32 elementSize);
};

class DxMesh {
public:
	Ptr<DxVertexBuffer> VertexBuffer;
	Ptr<DxIndexBuffer> IndexBuffer;
};

struct BufferGrave {
	DxResource Resource;

	DxCpuDescriptorHandle Srv;
	DxCpuDescriptorHandle Uav;
	DxCpuDescriptorHandle Clear;
	DxCpuDescriptorHandle GpuClear;
	DxCpuDescriptorHandle Raytracing;

	BufferGrave() {};
	BufferGrave(const BufferGrave&) = delete;
	BufferGrave(BufferGrave&&) = default;

	BufferGrave& operator=(const BufferGrave&) = delete;
	BufferGrave& operator=(BufferGrave&&) = default;

	~BufferGrave();
};
