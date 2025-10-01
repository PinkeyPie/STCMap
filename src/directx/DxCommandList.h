#pragma once

#include "dx.h"
#include "DxUploadBuffer.h"
#include "../pch.h"
#include "DxTexture.h"

class DxRenderTarget;

struct DxDynamicConstantBuffer {
	D3D12_GPU_VIRTUAL_ADDRESS GpuPtr;
	void* CpuPtr;
};

struct DxDynamicVertexBuffer {
	D3D12_VERTEX_BUFFER_VIEW View;
};

struct DxCommandAllocator {
	Com<ID3D12CommandAllocator> CommandAllocator;
	DxCommandAllocator* Next;
	uint64 LastExecutionFenceValue;
};

class DxCommandList {
public:
	D3D12_COMMAND_LIST_TYPE Type;
	DxCommandAllocator* CommandAllocator;
	DxGraphicsCommandList CommandList;
	uint64 UsedLastOnFrame;

	DxCommandList* Next;

	DxUploadBuffer UploadBuffer;

	ID3D12DescriptorHeap* DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	// Barriers
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

	// Shader resources.
	void SetDescriptorHeap(DxDescriptorHeap& descriptorHeap);
	void SetDescriptorHeap(DxDescriptorRange& descriptorRange);
	void SetGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);
	void SetGraphicsDescriptorTable(uint32 rootParameterIndex, DxDescriptorHandle handle);
	void SetComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);
	void SetComputeDescriptorTable(uint32 rootParameterIndex, DxDescriptorHandle handle);

	// Input assembly.
	void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void SetVertexBuffer(uint32 slot, DxVertexBuffer& buffer);
	void SetVertexBuffer(uint32 slot, DxDynamicVertexBuffer buffer);
	void SetIndexBuffer(DxIndexBuffer& buffer);

	// Rasterizer.
	void SetViewport(const D3D12_VIEWPORT& viewport);
	void SetScissor(const D3D12_RECT& scissor);

	// Render targets.
	void SetScreenRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, uint32 numRTVs, D3D12_CPU_DESCRIPTOR_HANDLE* dsv);
	void SetRenderTarget(DxRenderTarget& renderTarget, uint32 arraySlice = 0);
	void ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float* clearColor);
	void ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f);
	void ClearStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32 stencil = 0);
	void ClearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f, uint32 stencil = 0);
	void SetStencilReference(uint32 stencilReference);

	// Draw.
	void Draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance);
	void DrawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance);
	void DrawIndirect(DxCommandSignature commandSignature, uint32 numDraws, DxBuffer commandBuffer);
	void DrawIndirect(DxCommandSignature commandSignature, uint32 maxNumDraws, DxBuffer numDrawsBuffer, DxBuffer commandBuffer);

	// Dispatch.
	void Dispatch(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);
	void DispatchIndirect(DxCommandSignature commandSignature, uint32 numDispatches, DxBuffer commandBuffer);
	void DispatchIndirect(DxCommandSignature commandSignature, uint32 maxNumDispatches, DxBuffer numDispatchesBuffer, DxBuffer commandBuffer);

	// Raytracing.
	void Raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc);

	void Reset(DxCommandAllocator* commandAllocator);
};
