#pragma once

#include "../pch.h"
#include "dx.h"
#include "DxUploadBuffer.h"
#include "DxDynamicDescriptorHeap.h"
#include "DxDescriptorAllocation.h"
#include "DxTexture.h"
#include "DxBuffer.h"
#include "DxRenderTarget.h"

class DxCommandList {
public:
	DxCommandList(D3D12_COMMAND_LIST_TYPE type);

	D3D12_COMMAND_LIST_TYPE Type() { return _type; }

	ID3D12GraphicsCommandList4* CommandList() const { return _commandList.Get(); }

	// Barriers
	void Barriers(CD3DX12_RESOURCE_BARRIER* barriers, uint32 numBarriers);

	// Avoid calling these. Instead, use the dx_barrier_batcher interface to batch multiple barriers into a single submission.
	void TransitionBarrier(const Ptr<DxTexture> &texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void TransitionBarrier(const Ptr<DxBuffer> &buffer, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void TransitionBarrier(DxResource resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	void UavBarrier(const Ptr<DxTexture> &texture);
	void UavBarrier(const Ptr<DxBuffer> &buffer);
	void UavBarrier(DxResource resource);

	void AliasingBarrier(const Ptr<DxTexture> &before, const Ptr<DxTexture> &after);
	void AliasingBarrier(const Ptr<DxBuffer> &before, const Ptr<DxBuffer> &after);
	void AliasingBarrier(DxResource before, DxResource after);

	void AssertResourceState(const Ptr<DxTexture>& texture, D3D12_RESOURCE_STATES state, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void AssertResourceState(const Ptr<DxBuffer>& buffer, D3D12_RESOURCE_STATES state, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void AssertResourceState(DxResource resource, D3D12_RESOURCE_STATES state, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	// Copy.
	void CopyResource(DxResource from, DxResource to);
	void CopyTextureRegionToBuffer(const Ptr<DxTexture>& from, const Ptr<DxBuffer> &to, uint32 bufferElementOffset, uint32 x, uint32 y, uint32 width, uint32 height);
	void CopyTextureRegionToBuffer(const Ptr<DxTexture>& from, const Ptr<DxBuffer>& to, uint32 sourceX, uint32 sourceY, uint32 destX, uint32 destY, uint32 width, uint32 height);

	// Pipeline.
	void SetPipelineState(DxPipelineState pipelineState);
	void SetPipelineState(DxRaytracingPipelineState pipelineState);

	// Uniforms.
	void SetGraphicsRootSignature(const DxRootSignature& rootSignature);
	void SetGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T>
	void SetGraphics32BitConstants(uint32 rootParameterIndex, const T& constants);

	void SetComputeRootSignature(const DxRootSignature& rootSignature);
	void SetCompute32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T>
	void SetCompute32BitConstants(uint32 rootParameterIndex, const T& constants);

	DxAllocation AllocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	DxDynamicConstantBuffer UploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data);
	template <typename T>
	DxDynamicConstantBuffer UploadDynamicConstantBuffer(const T& data);

	DxDynamicConstantBuffer UploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data);
	template <typename T>
	DxDynamicConstantBuffer UploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, const T& data);

	void SetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, DxDynamicConstantBuffer address);

	DxDynamicConstantBuffer UploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data);
	template <typename T>
	DxDynamicConstantBuffer UploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, const T& data);

	void SetComputeDynamicConstantBuffer(uint32 rootParameterIndex, DxDynamicConstantBuffer address);

	DxDynamicVertexBuffer CreateDynamicVertexBuffer(uint32 elementSize, uint32 elementCount, void* data);

	void SetRootGraphicsUAV(uint32 rootParameterIndex, Ptr<DxBuffer> &buffer);
	void SetRootGraphicsUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
	void SetRootComputeUAV(uint32 rootParameterIndex, Ptr<DxBuffer> &buffer);
	void SetRootComputeUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);

	void SetRootGraphicsSRV(uint32 rootParameterIndex, Ptr<DxBuffer> &buffer);
	void SetRootGraphicsSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
	void SetRootComputeSRV(uint32 rootParameterIndex, Ptr<DxBuffer> &buffer);
	void SetRootComputeSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);

	void SetDescriptorHeapResource(uint32 rootParameterIndex, uint32 offset, uint32 count, DxCpuDescriptorHandle handle);
	void SetDescriptorHeapSRV(uint32 rootParameterIndex, uint32 offset, DxCpuDescriptorHandle handle) { SetDescriptorHeapResource(rootParameterIndex, offset, 1, handle); }
	void SetDescriptorHeapSRV(uint32 rootParameterIndex, uint32 offset, const Ptr<DxTexture> &texture) { SetDescriptorHeapResource(rootParameterIndex, offset, 1, texture->DefaultSRV()); }
	void SetDescriptorHeapSRV(uint32 rootParameterIndex, uint32 offset, const Ptr<DxBuffer> &buffer) { SetDescriptorHeapResource(rootParameterIndex, offset, 1, buffer->DefaultSRV); }
	void SetDescriptorHeapUAV(uint32 rootParameterIndex, uint32 offset, DxCpuDescriptorHandle handle) { SetDescriptorHeapResource(rootParameterIndex, offset, 1, handle); }
	void SetDescriptorHeapUAV(uint32 rootParameterIndex, uint32 offset, const Ptr<DxTexture> &texture) { SetDescriptorHeapResource(rootParameterIndex, offset, 1, texture->DefaultUAV()); }
	void SetDescriptorHeapUAV(uint32 rootParameterIndex, uint32 offset, const Ptr<DxBuffer> &buffer) { SetDescriptorHeapResource(rootParameterIndex, offset, 1, buffer->DefaultUAV); }

	// Shader resources.
	template<class T> void SetDescriptorHeap(DxDescriptorHeap<T>& descriptorHeap);
	void SetDescriptorHeap(DxDescriptorRange& descriptorRange);
	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* descriptorHeap);
	void ResetToDynamicDescriptorHeap();
	void SetGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);
	void SetComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);

	void ClearUAV(Ptr<DxBuffer> &buffer, float val = 0.f);
	void ClearUAV(DxResource resource, DxCpuDescriptorHandle cpuHandle, DxGpuDescriptorHandle gpuHandle, float val = 0.f);
	void ClearUAV(DxResource resource, DxCpuDescriptorHandle cpuHandle, DxGpuDescriptorHandle gpuHandle, uint32 val = 0);

	// Input assembly.
	void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void SetVertexBuffer(uint32 slot, const Ptr<DxVertexBuffer> &buffer);
	void SetVertexBuffer(uint32 slot, DxDynamicVertexBuffer buffer);
	void SetVertexBuffer(uint32 slot, D3D12_VERTEX_BUFFER_VIEW& buffer);
	void SetIndexBuffer(const Ptr<DxIndexBuffer> &buffer);
	void SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW& buffer);

	// Rasterizer.
	void SetViewport(const D3D12_VIEWPORT& viewport);
	void SetViewport(float x, float y, float width, float height, float minDepth = 0.f, float maxDepth = 1.f);
	void SetScissor(const D3D12_RECT& scissor);

	// Render targets.
	void SetRenderTarget(DxRtvDescriptorHandle* rtvs, uint32 numRTVs, DxDsvDescriptorHandle* dsv);
	void SetRenderTarget(DxRenderTarget& renderTarget);

	void ClearRTV(DxRtvDescriptorHandle rtv, float r, float g, float b, float a = 1.f);
	void ClearRTV(DxRtvDescriptorHandle rtv, const float* clearColor);
	void ClearRTV(const Ptr<DxTexture>& texture, float r, float g, float b, float a = 1.f);
	void ClearRTV(const Ptr<DxTexture>& texture, const float* clearColor);
	void ClearRTV(DxRenderTarget& renderTarget, uint32 attachment, const float* clearColor);
	void ClearRTV(DxRenderTarget& renderTarget, uint32 attachment, float r, float g, float b, float a = 1.f);

	void ClearDepth(DxDsvDescriptorHandle dsv, float depth = 1.f);
	void ClearDepth(DxRenderTarget& renderTarget, float depth = 1.f);

	void ClearStencil(DxDsvDescriptorHandle dsv, uint32 stencil = 0);
	void ClearStencil(DxRenderTarget& renderTarget, uint32 stencil = 0);

	void ClearDepthAndStencil(DxDsvDescriptorHandle dsv, float depth = 1.f, uint32 stencil = 0);
	void ClearDepthAndStencil(DxRenderTarget& renderTarget, float depth = 1.f, uint32 stencil = 0);

	void SetStencilReference(uint32 stencilReference);
	void SetBlendFactor(const float* blendFactor);
	void ResolveSubresource(DxResource dst, UINT dstSubresource, DxResource src, UINT srcSubresource, DXGI_FORMAT format);

	// Draw.
	void Draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance);
	void DrawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance);
	void DrawIndirect(DxCommandSignature commandSignature, uint32 numDraws, const Ptr<DxBuffer> &commandBuffer);
	void DrawIndirect(DxCommandSignature commandSignature, uint32 maxNumDraws, const Ptr<DxBuffer> &numDrawsBuffer,
		const Ptr<DxBuffer> &commandBuffer);
	void DrawFullscreenTriangle();
	void DrawCubeTriangleStrip();

	// Dispatch.
	void Dispatch(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);
	void DispatchIndirect(DxCommandSignature commandSignature, uint32 numDispatches, const Ptr<DxBuffer> &commandBuffer);
	void DispatchIndirect(DxCommandSignature commandSignature, uint32 maxNumDispatches, const Ptr<DxBuffer> &numDispatchesBuffer,
		const Ptr<DxBuffer> &commandBuffer);

	// Mesh shaders
	void DispatchMesh(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);

	// Raytracing.
	void Raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc);

	// Queries
	void QueryTimestamp(uint32 index);

	void Reset();

private:
	D3D12_COMMAND_LIST_TYPE _type;

	DxCommandAllocator _commandAllocator = nullptr;
	DxGraphicsCommandList _commandList = nullptr;

	DxUploadBuffer _uploadBuffer;
	DxDynamicDescriptorHeap _dynamicDescriptorHeap;
	ID3D12DescriptorHeap* _descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	DxCommandList* _next;
	uint64 _lastExecutionFenceValue;

	DxQueryHeap _timeStampQueryHeap;

	friend class DxContext;
	friend class DxCommandQueue;
};

template<class T>
void DxCommandList::SetDescriptorHeap(DxDescriptorHeap<T> &descriptorHeap) {
	_descriptorHeaps[descriptorHeap.Type] = descriptorHeap.DescriptorHeap();

	uint32 numDescriptorHeaps = 0;
	ID3D12DescriptorHeap* heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
		ID3D12DescriptorHeap* heap = _descriptorHeaps[i];
		if (heap) {
			heaps[numDescriptorHeaps++] = heap;
		}
	}

	_commandList->SetDescriptorHeaps(numDescriptorHeaps, heaps);
}


template <typename T>
void DxCommandList::SetGraphics32BitConstants(uint32 rootParameterIndex, const T& constants) {
	static_assert(sizeof(T) % 4 == 0, "Size of type must be a multiple of 4 bytes.");
	SetGraphics32BitConstants(rootParameterIndex, sizeof(T) / 4, &constants);
}

template <typename T>
void DxCommandList::SetCompute32BitConstants(uint32 rootParameterIndex, const T& constants) {
	static_assert(sizeof(T) % 4 == 0, "Size of type must be a multiple of 4 bytes.");
	SetCompute32BitConstants(rootParameterIndex, sizeof(T) / 4, &constants);
}

template <typename T>
DxDynamicConstantBuffer DxCommandList::UploadDynamicConstantBuffer(const T& data) {
	return UploadDynamicConstantBuffer(sizeof(T), &data);
}

template <typename T>
DxDynamicConstantBuffer DxCommandList::UploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, const T& data) {
	return UploadAndSetGraphicsDynamicConstantBuffer(rootParameterIndex, sizeof(T), &data);
}

template <typename T>
DxDynamicConstantBuffer DxCommandList::UploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, const T& data) {
	return UploadAndSetComputeDynamicConstantBuffer(rootParameterIndex, sizeof(T), &data);
}