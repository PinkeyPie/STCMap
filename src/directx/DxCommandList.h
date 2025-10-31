#pragma once

#include "dx.h"
#include "DxUploadBuffer.h"
#include "../pch.h"
#include "DxTexture.h"

class DxRenderTarget;

struct DxCommandAllocator {
	Com<ID3D12CommandAllocator> CommandAllocator;
	DxCommandAllocator* Next;
	uint64 LastExecutionFenceValue;
};

class DxCommandList {
public:
	D3D12_COMMAND_LIST_TYPE Type;

	ID3D12GraphicsCommandList4* CommandList() const { return _commandList.Get(); }

	// Barriers
	void Barriers(CD3DX12_RESOURCE_BARRIER* barriers, uint32 numBarriers);

	void TransitionBarrier(DxTexture& texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void TransitionBarrier(DxBuffer& buffer, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void TransitionBarrier(DxResource resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	void UavBarrier(DxTexture& texture);
	void UavBarrier(DxBuffer& buffer);
	void UavBarrier(DxResource resource);

	void AliasingBarrier(DxTexture& before, DxTexture& after);
	void AliasingBarrier(DxBuffer& before, DxBuffer& after);
	void AliasingBarrier(DxResource before, DxResource after);

	// Copy.
	void CopyResource(DxResource from, DxResource to);

	// Pipeline.
	void SetPipelineState(DxPipelineState pipelineState);
	void SetPipelineState(DxRaytracingPipelineState pipelineState);

	// Uniforms.
	void SetGraphicsRootSignature(DxRootSignature rootSignature);
	void SetGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T>
	void SetGraphics32BitConstants(uint32 rootParameterIndex, const T& constants);

	void SetComputeRootSignature(DxRootSignature rootSignature);
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

	void SetGraphicsUAV(uint32 rootParameterIndex, DxBuffer& buffer);
	void SetGraphicsUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
	void SetComputeUAV(uint32 rootParameterIndex, DxBuffer& buffer);
	void SetComputeUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);

	void SetGraphicsSRV(uint32 rootParameterIndex, DxBuffer& buffer);
	void SetGraphicsSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
	void SetComputeSRV(uint32 rootParameterIndex, DxBuffer& buffer);
	void SetComputeSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);

	// Shader resources.
	void SetDescriptorHeap(DxDescriptorHeap& descriptorHeap);
	void SetDescriptorHeap(DxDescriptorRange& descriptorRange);
	void SetGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);
	void SetGraphicsDescriptorTable(uint32 rootParameterIndex, DxDescriptorHandle handle);
	void SetComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);
	void SetComputeDescriptorTable(uint32 rootParameterIndex, DxDescriptorHandle handle);

	void ClearUAV(DxResource resource, CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle, float val = 0.f);
	void ClearUAV(DxResource resource, CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle, uint32 val = 0);

	// Input assembly.
	void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void SetVertexBuffer(uint32 slot, DxVertexBuffer& buffer);
	void SetVertexBuffer(uint32 slot, DxDynamicVertexBuffer buffer);
	void SetVertexBuffer(uint32 slot, D3D12_VERTEX_BUFFER_VIEW& buffer);
	void SetIndexBuffer(DxIndexBuffer& buffer);
	void SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW& buffer);

	// Rasterizer.
	void SetViewport(const D3D12_VIEWPORT& viewport);
	void SetScissor(const D3D12_RECT& scissor);

	// Render targets.
	void SetScreenRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, uint32 numRTVs, D3D12_CPU_DESCRIPTOR_HANDLE* dsv);
	void SetRenderTarget(DxRenderTarget& renderTarget, uint32 arraySlice = 0);
	void ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float r, float g, float b, float a = 1.f);
	void ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const float* clearColor);
	void ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f);
	void ClearStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32 stencil = 0);
	void ClearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f, uint32 stencil = 0);
	void SetStencilReference(uint32 stencilReference);
	void SetBlendFactor(const float* blendFactor);
	void ResolveSubresource(DxResource dst, UINT dstSubresource, DxResource src, UINT srcSubresource, DXGI_FORMAT format);

	// Draw.
	void Draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance);
	void DrawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance);
	void DrawIndirect(DxCommandSignature commandSignature, uint32 numDraws, DxBuffer commandBuffer);
	void DrawIndirect(DxCommandSignature commandSignature, uint32 maxNumDraws, DxBuffer numDrawsBuffer, DxBuffer commandBuffer);
	void DrawFullscreenTriangle();

	// Dispatch.
	void Dispatch(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);
	void DispatchIndirect(DxCommandSignature commandSignature, uint32 numDispatches, DxBuffer commandBuffer);
	void DispatchIndirect(DxCommandSignature commandSignature, uint32 maxNumDispatches, DxBuffer numDispatchesBuffer, DxBuffer commandBuffer);

	// Raytracing.
	void Raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc);

	void Reset(DxCommandAllocator* commandAllocator);

private:
	ID3D12DescriptorHeap* _descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	DxCommandAllocator* _commandAllocator = nullptr;
	DxGraphicsCommandList _commandList = nullptr;
	DxUploadBuffer _uploadBuffer;
	DxCommandList* _next;
	uint64 _usedLastOnFrame;

	friend class DxContext;
	friend class DxCommandQueue;
};

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