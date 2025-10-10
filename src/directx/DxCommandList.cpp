#include "DxCommandList.h"
#include "DxRenderTarget.h"

void DxCommandList::Barriers(CD3DX12_RESOURCE_BARRIER* barriers, uint32 numBarriers) {
	CommandList->ResourceBarrier(numBarriers, barriers);
}

void DxCommandList::TransitionBarrier(DxTexture& texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	TransitionBarrier(texture.Resource, from, to, subresource);
}

void DxCommandList::TransitionBarrier(DxBuffer& buffer, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	TransitionBarrier(buffer.Resource, from, to, subresource);
}

void DxCommandList::TransitionBarrier(DxResource resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), from, to, subresource);
	CommandList->ResourceBarrier(1, &barrier);
}

void DxCommandList::UavBarrier(DxTexture& texture) {
	UavBarrier(texture.Resource);
}

void DxCommandList::UavBarrier(DxBuffer& buffer) {
	UavBarrier(buffer.Resource);
}

void DxCommandList::UavBarrier(DxResource resource) {
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	CommandList->ResourceBarrier(1, &barrier);
}

void DxCommandList::AliasingBarrier(DxTexture& before, DxTexture& after) {
	AliasingBarrier(before.Resource, after.Resource);
}

void DxCommandList::AliasingBarrier(DxBuffer& before, DxBuffer& after) {
	AliasingBarrier(before.Resource, after.Resource);
}

void DxCommandList::AliasingBarrier(DxResource before, DxResource after) {
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(before.Get(), after.Get());
	CommandList->ResourceBarrier(1, &barrier);
}

void DxCommandList::CopyResource(DxResource from, DxResource to) {
	CommandList->CopyResource(to.Get(), from.Get());
}

void DxCommandList::SetPipelineState(DxPipelineState pipelineState) {
	CommandList->SetPipelineState(pipelineState.Get());
}

void DxCommandList::SetPipelineState(DxRaytracingPipelineState pipelineState) {
	CommandList->SetPipelineState1(pipelineState.Get());
}

void DxCommandList::SetGraphicsRootSignature(DxRootSignature rootSignature) {
	CommandList->SetGraphicsRootSignature(rootSignature.Get());
}

void DxCommandList::SetGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants) {
	CommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void DxCommandList::SetComputeRootSignature(DxRootSignature rootSignature) {
	CommandList->SetComputeRootSignature(rootSignature.Get());
}

void DxCommandList::SetCompute32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants) {
	CommandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

DxAllocation DxCommandList::AllocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment) {
	DxAllocation allocation = UploadBuffer.Allocate(sizeInBytes, alignment);
	return allocation;
}

DxDynamicConstantBuffer DxCommandList::UploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data) {
	DxAllocation allocation = AllocateDynamicBuffer(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(allocation.CpuPtr, data, sizeInBytes);
	return { allocation.GpuPtr, allocation.CpuPtr };
}

DxDynamicConstantBuffer DxCommandList::UploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data) {
	DxDynamicConstantBuffer address = UploadDynamicConstantBuffer(sizeInBytes, data);
	CommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, address.GpuPtr);
	return address;
}

void DxCommandList::SetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, DxDynamicConstantBuffer address) {
	CommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, address.GpuPtr);
}

DxDynamicConstantBuffer DxCommandList::UploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data) {
	DxDynamicConstantBuffer address = UploadDynamicConstantBuffer(sizeInBytes, data);
	CommandList->SetComputeRootConstantBufferView(rootParameterIndex, address.GpuPtr);
	return address;
}

void DxCommandList::SetComputeDynamicConstantBuffer(uint32 rootParameterIndex, DxDynamicConstantBuffer address) {
	CommandList->SetComputeRootConstantBufferView(rootParameterIndex, address.GpuPtr);
}

DxDynamicVertexBuffer DxCommandList::CreateDynamicVertexBuffer(uint32 elementSize, uint32 elementCount, void* data) {
	uint32 sizeInBytes = elementSize * elementCount;
	DxAllocation allocation = AllocateDynamicBuffer(sizeInBytes, elementSize);
	memcpy(allocation.CpuPtr, data, sizeInBytes);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = allocation.GpuPtr;
	vertexBufferView.SizeInBytes = sizeInBytes;
	vertexBufferView.StrideInBytes = elementSize;

	return { vertexBufferView };
}

void DxCommandList::SetDescriptorHeap(DxDescriptorHeap& descriptorHeap) {
	DescriptorHeaps[descriptorHeap.Type] = descriptorHeap.DescriptorHeap.Get();

	uint32 numDescriptorHeaps = 0;
	ID3D12DescriptorHeap* heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
		ID3D12DescriptorHeap* heap = DescriptorHeaps[i];
		if (heap) {
			heaps[numDescriptorHeaps++] = heap;
		}
	}

	CommandList->SetDescriptorHeaps(numDescriptorHeaps, heaps);
}

void DxCommandList::SetDescriptorHeap(DxDescriptorRange& descriptorRange) {
	DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = descriptorRange.DescriptorHeap.Get();

	uint32 numDescriptorHeaps = 0;
	ID3D12DescriptorHeap* heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};
	for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
		ID3D12DescriptorHeap* heap = DescriptorHeaps[i];
		if (heap) {
			heaps[numDescriptorHeaps++] = heap;
		}
	}

	CommandList->SetDescriptorHeaps(numDescriptorHeaps, heaps);
}

void DxCommandList::SetGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle) {
	CommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, handle);
}

void DxCommandList::SetGraphicsDescriptorTable(uint32 rootParameterIndex, DxDescriptorHandle handle) {
	SetGraphicsDescriptorTable(rootParameterIndex, handle.GpuHandle);
}

void DxCommandList::SetComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle) {
	CommandList->SetComputeRootDescriptorTable(rootParameterIndex, handle);
}

void DxCommandList::SetComputeDescriptorTable(uint32 rootParameterIndex, DxDescriptorHandle handle) {
	SetComputeDescriptorTable(rootParameterIndex, handle.GpuHandle);
}

void DxCommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology) {
	CommandList->IASetPrimitiveTopology(topology);
}

void DxCommandList::SetVertexBuffer(uint32 slot, DxDynamicVertexBuffer buffer) {
	CommandList->IASetVertexBuffers(slot, 1, &buffer.View);
}

void DxCommandList::SetVertexBuffer(uint32 slot, DxVertexBuffer& buffer) {
	CommandList->IASetVertexBuffers(slot, 1, &buffer.View);
}

void DxCommandList::SetVertexBuffer(uint32 slot, D3D12_VERTEX_BUFFER_VIEW& buffer) {
	CommandList->IASetVertexBuffers(slot, 1, &buffer);
}

void DxCommandList::SetIndexBuffer(DxIndexBuffer& buffer) {
	CommandList->IASetIndexBuffer(&buffer.View);
}

void DxCommandList::SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW& buffer) {
	CommandList->IASetIndexBuffer(&buffer);
}

void DxCommandList::SetViewport(const D3D12_VIEWPORT& viewport) {
	CommandList->RSSetViewports(1, &viewport);
}

void DxCommandList::SetScissor(const D3D12_RECT& scissor) {
	CommandList->RSSetScissorRects(1, &scissor);
}

void DxCommandList::SetScreenRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, uint32 numRTVs, D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
	CommandList->OMSetRenderTargets(numRTVs, rtvs, FALSE, dsv);
}

void DxCommandList::SetRenderTarget(DxRenderTarget& renderTarget, uint32 arraySlice) {
	D3D12_CPU_DESCRIPTOR_HANDLE* dsv = (renderTarget.DepthStencilFormat != DXGI_FORMAT_UNKNOWN) ? &renderTarget.DsvHandle : nullptr;
	CommandList->OMSetRenderTargets(renderTarget.NumAttachments, renderTarget.RtvHandles, FALSE, dsv);
}

void DxCommandList::ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const float* clearColor) {
	CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void DxCommandList::ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth) {
	CommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void DxCommandList::ClearStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32 stencil) {
	CommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_STENCIL, 0.f, stencil, 0, nullptr);
}

void DxCommandList::ClearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth, uint32 stencil) {
	CommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
}

void DxCommandList::SetStencilReference(uint32 stencilReference) {
	CommandList->OMSetStencilRef(stencilReference);
}

void DxCommandList::SetBlendFactor(const float* blendFactor) {
	CommandList->OMSetBlendFactor(blendFactor);
}

void DxCommandList::ResolveSubresource(DxResource dst, UINT dstSubresource, DxResource src, UINT srcSubresource, DXGI_FORMAT format) {
	CommandList->ResolveSubresource(dst.Get(), dstSubresource, src.Get(), srcSubresource, format);
}

void DxCommandList::Draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance) {
	CommandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void DxCommandList::DrawFullscreenTriangle() {
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Draw(3, 1, 0, 0);
}

void DxCommandList::DrawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance) {
	CommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void DxCommandList::DrawIndirect(DxCommandSignature commandSignature, uint32 numDraws, DxBuffer commandBuffer) {
	CommandList->ExecuteIndirect(
		commandSignature.Get(),
		numDraws,
		commandBuffer.Resource.Get(),
		0,
		nullptr,
		0
	);
}

void DxCommandList::DrawIndirect(DxCommandSignature commandSignature, uint32 maxNumDraws, DxBuffer numDrawsBuffer, DxBuffer commandBuffer) {
	CommandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumDraws,
		commandBuffer.Resource.Get(),
		0,
		numDrawsBuffer.Resource.Get(),
		0
	);
}

void DxCommandList::Dispatch(uint32 numGroupsX, uint32 numGroupsY, uint32 numGroupsZ) {
	CommandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

void DxCommandList::DispatchIndirect(DxCommandSignature commandSignature, uint32 numDispatches, DxBuffer commandBuffer) {
	CommandList->ExecuteIndirect(
		commandSignature.Get(),
		numDispatches,
		commandBuffer.Resource.Get(),
		0,
		nullptr,
		0
	);
}

void DxCommandList::DispatchIndirect(DxCommandSignature commandSignature, uint32 maxNumDispatches, DxBuffer numDispatchesBuffer, DxBuffer commandBuffer) {
	CommandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumDispatches,
		commandBuffer.Resource.Get(),
		0,
		numDispatchesBuffer.Resource.Get(),
		0
	);
}

void DxCommandList::Raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc) {
	CommandList->DispatchRays(&raytraceDesc);
}

void DxCommandList::Reset(DxCommandAllocator* commandAllocator) {
	CommandAllocator = commandAllocator;
	ThrowIfFailed(CommandList->Reset(commandAllocator->CommandAllocator.Get(), nullptr));

	for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
		DescriptorHeaps[i] = nullptr;
	}

	UploadBuffer.Reset();
}
