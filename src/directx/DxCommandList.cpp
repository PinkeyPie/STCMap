#include "DxCommandList.h"
#include "DxRenderTarget.h"
#include "DxPipeline.h"

DxCommandList::DxCommandList(D3D12_COMMAND_LIST_TYPE type) {
	_type = type;
	DxContext& context = DxContext::Instance();
	ThrowIfFailed(context.GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(_commandAllocator.GetAddressOf())));
	ThrowIfFailed(context.GetDevice()->CreateCommandList(0, type, _commandAllocator.Get(), 0, IID_PPV_ARGS(_commandList.GetAddressOf())));

	_dynamicDescriptorHeap.Initialize();
}


void DxCommandList::Barriers(CD3DX12_RESOURCE_BARRIER* barriers, uint32 numBarriers) {
	_commandList->ResourceBarrier(numBarriers, barriers);
}

void DxCommandList::TransitionBarrier(const Ptr<DxTexture> &texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	TransitionBarrier(texture->Resource, from, to, subresource);
}

void DxCommandList::TransitionBarrier(const Ptr<DxBuffer> &buffer, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	TransitionBarrier(buffer->Resource, from, to, subresource);
}

void DxCommandList::TransitionBarrier(DxResource resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), from, to, subresource);
	_commandList->ResourceBarrier(1, &barrier);
}

void DxCommandList::UavBarrier(const Ptr<DxTexture> &texture) {
	UavBarrier(texture->Resource);
}

void DxCommandList::UavBarrier(const Ptr<DxBuffer> &buffer) {
	UavBarrier(buffer->Resource);
}

void DxCommandList::UavBarrier(DxResource resource) {
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	_commandList->ResourceBarrier(1, &barrier);
}

void DxCommandList::AliasingBarrier(const Ptr<DxTexture> &before, const Ptr<DxTexture> &after) {
	AliasingBarrier(before->Resource.Get(), after->Resource.Get());
}

void DxCommandList::AliasingBarrier(const Ptr<DxBuffer> &before, const Ptr<DxBuffer> &after) {
	AliasingBarrier(before->Resource, after->Resource);
}

void DxCommandList::AliasingBarrier(DxResource before, DxResource after) {
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(before.Get(), after.Get());
	_commandList->ResourceBarrier(1, &barrier);
}

void DxCommandList::AssertResourceState(const Ptr<DxTexture> &texture, D3D12_RESOURCE_STATES state, uint32 subresource) {
	AssertResourceState(texture->Resource.Get(), state, subresource);
}

void DxCommandList::AssertResourceState(const Ptr<DxBuffer> &buffer, D3D12_RESOURCE_STATES state, uint32 subresource) {
	AssertResourceState(buffer->Resource, state, subresource);
}

void DxCommandList::AssertResourceState(DxResource resource, D3D12_RESOURCE_STATES state, uint32 subresource) {
#ifdef _DEBUG
	ID3D12DebugCommandList* debugCl = nullptr;
	if (SUCCEEDED(_commandList->QueryInterface(IID_PPV_ARGS(&debugCl)))) {
		assert(debugCl->AssertResourceState(resource.Get(), subresource, state));
	}
#endif
}

void DxCommandList::CopyResource(DxResource from, DxResource to) {
	_commandList->CopyResource(to.Get(), from.Get());
}

void DxCommandList::CopyTextureRegionToBuffer(const Ptr<DxTexture> &from, const Ptr<DxBuffer> &to, uint32 bufferElementOffset, uint32 x, uint32 y, uint32 width, uint32 height) {
	assert(to->ElementCount >= width * height);
	assert(to->ElementSize == DxTexture::GetFormatSize(from->Format));

	uint32 totalSize = to->ElementCount * to->ElementSize;

	D3D12_TEXTURE_COPY_LOCATION descLocation = { to->Resource.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT{0, D3D12_SUBRESOURCE_FOOTPRINT{from->Format, to->ElementCount, 1, 1, AlignTo(totalSize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)}}};

	D3D12_TEXTURE_COPY_LOCATION srcLocation = {from->Resource.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, 0 };

	D3D12_BOX srcBox = { x, y, 0, x + width, y + height, 1 };
	_commandList->CopyTextureRegion(&descLocation, bufferElementOffset, 0, 0, &srcLocation, &srcBox);
}

void DxCommandList::CopyTextureRegionToBuffer(const Ptr<DxTexture> &from, const Ptr<DxBuffer> &to, uint32 sourceX, uint32 sourceY, uint32 destX, uint32 destY, uint32 width, uint32 height) {
	D3D12_TEXTURE_COPY_LOCATION destLocation = { to->Resource.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, 0 };
	D3D12_TEXTURE_COPY_LOCATION srcLocation = { from->Resource.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, 0 };

	D3D12_BOX srcBox = { sourceX, sourceY, 0, sourceX + width, sourceY + height, 1 };
	_commandList->CopyTextureRegion(&destLocation, destX, destY, 0, &srcLocation, &srcBox);
}


void DxCommandList::SetPipelineState(DxPipelineState pipelineState) {
	_commandList->SetPipelineState(pipelineState.Get());
}

void DxCommandList::SetPipelineState(DxRaytracingPipelineState pipelineState) {
	_commandList->SetPipelineState1(pipelineState.Get());
}

void DxCommandList::SetGraphicsRootSignature(const DxRootSignature& rootSignature) {
	_dynamicDescriptorHeap.ParseRootSignature(rootSignature);
	_commandList->SetGraphicsRootSignature(rootSignature.RootSignature());
}

void DxCommandList::SetGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants) {
	_commandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void DxCommandList::SetComputeRootSignature(const DxRootSignature& rootSignature) {
	_dynamicDescriptorHeap.ParseRootSignature(rootSignature);
	_commandList->SetComputeRootSignature(rootSignature.RootSignature());
}

void DxCommandList::SetCompute32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants) {
	_commandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

DxAllocation DxCommandList::AllocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment) {
	DxAllocation allocation = _uploadBuffer.Allocate(sizeInBytes, alignment);
	return allocation;
}

DxDynamicConstantBuffer DxCommandList::UploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data) {
	DxAllocation allocation = AllocateDynamicBuffer(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(allocation.CpuPtr, data, sizeInBytes);
	return { allocation.GpuPtr, allocation.CpuPtr };
}

DxDynamicConstantBuffer DxCommandList::UploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data) {
	DxDynamicConstantBuffer address = UploadDynamicConstantBuffer(sizeInBytes, data);
	_commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, address.GpuPtr);
	return address;
}

void DxCommandList::SetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, DxDynamicConstantBuffer address) {
	_commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, address.GpuPtr);
}

DxDynamicConstantBuffer DxCommandList::UploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data) {
	DxDynamicConstantBuffer address = UploadDynamicConstantBuffer(sizeInBytes, data);
	_commandList->SetComputeRootConstantBufferView(rootParameterIndex, address.GpuPtr);
	return address;
}

void DxCommandList::SetComputeDynamicConstantBuffer(uint32 rootParameterIndex, DxDynamicConstantBuffer address) {
	_commandList->SetComputeRootConstantBufferView(rootParameterIndex, address.GpuPtr);
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

void DxCommandList::SetRootGraphicsUAV(uint32 rootParameterIndex, Ptr<DxBuffer> &buffer) {
	_commandList->SetGraphicsRootUnorderedAccessView(rootParameterIndex, buffer->GpuVirtualAddress);
}

void DxCommandList::SetRootGraphicsUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address) {
	_commandList->SetGraphicsRootUnorderedAccessView(rootParameterIndex, address);
}

void DxCommandList::SetRootComputeUAV(uint32 rootParameterIndex, Ptr<DxBuffer> &buffer) {
	_commandList->SetComputeRootUnorderedAccessView(rootParameterIndex, buffer->GpuVirtualAddress);
}

void DxCommandList::SetRootComputeUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address) {
	_commandList->SetComputeRootUnorderedAccessView(rootParameterIndex, address);
}

void DxCommandList::SetRootGraphicsSRV(uint32 rootParameterIndex, Ptr<DxBuffer> &buffer) {
	_commandList->SetGraphicsRootShaderResourceView(rootParameterIndex, buffer->GpuVirtualAddress);
}

void DxCommandList::SetRootGraphicsSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address) {
	_commandList->SetGraphicsRootShaderResourceView(rootParameterIndex, address);
}

void DxCommandList::SetRootComputeSRV(uint32 rootParameterIndex, Ptr<DxBuffer> &buffer) {
	_commandList->SetComputeRootShaderResourceView(rootParameterIndex, buffer->GpuVirtualAddress);
}

void DxCommandList::SetRootComputeSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address) {
	_commandList->SetComputeRootShaderResourceView(rootParameterIndex, address);
}

void DxCommandList::SetDescriptorHeapResource(uint32 rootParameterIndex, uint32 offset, uint32 count, DxCpuDescriptorHandle handle) {
	_dynamicDescriptorHeap.StageDescriptors(rootParameterIndex, offset, count, (CD3DX12_CPU_DESCRIPTOR_HANDLE)handle);
}

void DxCommandList::SetDescriptorHeap(DxDescriptorRange& descriptorRange) {
	_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = descriptorRange.DescriptorHeap.Get();

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

void DxCommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap *descriptorHeap) {
	_descriptorHeaps[type] = descriptorHeap;

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

void DxCommandList::ResetToDynamicDescriptorHeap() {
	_dynamicDescriptorHeap.SetCurrentDescriptorHeap(this);
}


void DxCommandList::SetGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle) {
	_commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, handle);
}

void DxCommandList::SetComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle) {
	_commandList->SetComputeRootDescriptorTable(rootParameterIndex, handle);
}

void DxCommandList::ClearUAV(Ptr<DxBuffer> &buffer, float val) {
	ClearUAV(buffer->Resource, buffer->CpuClearUAV, buffer->GpuClearUAV, val);
}

void DxCommandList::ClearUAV(DxResource resource, DxCpuDescriptorHandle cpuHandle, DxGpuDescriptorHandle gpuHandle, float val) {
	float vals[] = { val, val, val, val };
	_commandList->ClearUnorderedAccessViewFloat(gpuHandle, cpuHandle, resource.Get(), vals, 0, 0);
}

void DxCommandList::ClearUAV(DxResource resource, DxCpuDescriptorHandle cpuHandle, DxGpuDescriptorHandle gpuHandle, uint32 val) {
	uint32 vals[] = { val, val, val, val };
	_commandList->ClearUnorderedAccessViewUint(gpuHandle, cpuHandle, resource.Get(), vals, 0, 0);
}

void DxCommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology) {
	_commandList->IASetPrimitiveTopology(topology);
}

void DxCommandList::SetVertexBuffer(uint32 slot, DxDynamicVertexBuffer buffer) {
	_commandList->IASetVertexBuffers(slot, 1, &buffer.View);
}

void DxCommandList::SetVertexBuffer(uint32 slot, const Ptr<DxVertexBuffer> &buffer) {
	_commandList->IASetVertexBuffers(slot, 1, &buffer->View);
}

void DxCommandList::SetVertexBuffer(uint32 slot, D3D12_VERTEX_BUFFER_VIEW& buffer) {
	_commandList->IASetVertexBuffers(slot, 1, &buffer);
}

void DxCommandList::SetIndexBuffer(const Ptr<DxIndexBuffer> &buffer) {
	_commandList->IASetIndexBuffer(&buffer->View);
}

void DxCommandList::SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW& buffer) {
	_commandList->IASetIndexBuffer(&buffer);
}

void DxCommandList::SetViewport(const D3D12_VIEWPORT& viewport) {
	_commandList->RSSetViewports(1, &viewport);
}

void DxCommandList::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
	D3D12_VIEWPORT viewport = { x, y, width, height, minDepth, maxDepth };
	SetViewport(viewport);
}

void DxCommandList::SetScissor(const D3D12_RECT& scissor) {
	_commandList->RSSetScissorRects(1, &scissor);
}

void DxCommandList::SetRenderTarget(DxRtvDescriptorHandle* rtvs, uint32 numRTVs, DxDsvDescriptorHandle* dsv) {
	_commandList->OMSetRenderTargets(numRTVs, (D3D12_CPU_DESCRIPTOR_HANDLE*)rtvs, FALSE, (D3D12_CPU_DESCRIPTOR_HANDLE*)dsv);
}

void DxCommandList::SetRenderTarget(DxRenderTarget& renderTarget) {
	SetRenderTarget(renderTarget.RTV, renderTarget.NumAttachments, renderTarget.DSV ? &renderTarget.DSV : 0);
}

void DxCommandList::ClearRTV(DxRtvDescriptorHandle rtv, float r, float g, float b, float a) {
	float clearColor[] = { r, g, b, a };
	ClearRTV(rtv, clearColor);
}

void DxCommandList::ClearRTV(DxRtvDescriptorHandle rtv, const float* clearColor) {
	_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void DxCommandList::ClearRTV(const Ptr<DxTexture> &texture, float r, float g, float b, float a) {
	ClearRTV(texture->RtvHandles, r, g, b, a);
}

void DxCommandList::ClearRTV(const Ptr<DxTexture> &texture, const float *clearColor) {
	ClearRTV(texture->RtvHandles, clearColor);
}

void DxCommandList::ClearRTV(DxRenderTarget &renderTarget, uint32 attachment, const float *clearColor) {
	ClearRTV(renderTarget.RTV[attachment], clearColor);
}

void DxCommandList::ClearRTV(DxRenderTarget &renderTarget, uint32 attachment, float r, float g, float b, float a) {
	ClearRTV(renderTarget.RTV[attachment], r, g, b, a);
}

void DxCommandList::ClearDepth(DxDsvDescriptorHandle dsv, float depth) {
	_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void DxCommandList::ClearDepth(DxRenderTarget &renderTarget, float depth) {
	ClearDepth(renderTarget.DSV, depth);
}

void DxCommandList::ClearStencil(DxDsvDescriptorHandle dsv, uint32 stencil) {
	_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_STENCIL, 0.f, stencil, 0, nullptr);
}

void DxCommandList::ClearStencil(DxRenderTarget &renderTarget, uint32 stencil) {
	ClearStencil(renderTarget.DSV, stencil);
}

void DxCommandList::ClearDepthAndStencil(DxDsvDescriptorHandle dsv, float depth, uint32 stencil) {
	_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
}

void DxCommandList::ClearDepthAndStencil(DxRenderTarget &renderTarget, float depth, uint32 stencil) {
	ClearDepthAndStencil(renderTarget.DSV, depth, stencil);
}

void DxCommandList::SetStencilReference(uint32 stencilReference) {
	_commandList->OMSetStencilRef(stencilReference);
}

void DxCommandList::SetBlendFactor(const float* blendFactor) {
	_commandList->OMSetBlendFactor(blendFactor);
}

void DxCommandList::ResolveSubresource(DxResource dst, UINT dstSubresource, DxResource src, UINT srcSubresource, DXGI_FORMAT format) {
	_commandList->ResolveSubresource(dst.Get(), dstSubresource, src.Get(), srcSubresource, format);
}

void DxCommandList::Draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance) {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	_commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void DxCommandList::DrawFullscreenTriangle() {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Draw(3, 1, 0, 0);
}

void DxCommandList::DrawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance) {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	_commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void DxCommandList::DrawIndirect(DxCommandSignature commandSignature, uint32 numDraws, const Ptr<DxBuffer> &commandBuffer) {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	_commandList->ExecuteIndirect(
		commandSignature.Get(),
		numDraws,
		commandBuffer->Resource.Get(),
		0,
		nullptr,
		0
	);
}

void DxCommandList::DrawIndirect(DxCommandSignature commandSignature, uint32 maxNumDraws, const Ptr<DxBuffer> &numDrawsBuffer, const Ptr<DxBuffer> &
                                 commandBuffer) {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	_commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumDraws,
		commandBuffer->Resource.Get(),
		0,
		numDrawsBuffer->Resource.Get(),
		0
	);
}

void DxCommandList::DrawCubeTriangleStrip() {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Draw(14, 1, 0, 0);
}

void DxCommandList::Dispatch(uint32 numGroupsX, uint32 numGroupsY, uint32 numGroupsZ) {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDispatch(this);
	_commandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

void DxCommandList::DispatchIndirect(DxCommandSignature commandSignature, uint32 numDispatches, const Ptr<DxBuffer> &commandBuffer) {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDispatch(this);
	_commandList->ExecuteIndirect(
		commandSignature.Get(),
		numDispatches,
		commandBuffer->Resource.Get(),
		0,
		nullptr,
		0
	);
}

void DxCommandList::DispatchIndirect(DxCommandSignature commandSignature, uint32 maxNumDispatches, const Ptr<DxBuffer> &numDispatchesBuffer, const Ptr<
                                     DxBuffer> &commandBuffer) {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDispatch(this);
	_commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumDispatches,
		commandBuffer->Resource.Get(),
		0,
		numDispatchesBuffer->Resource.Get(),
		0
	);
}

void DxCommandList::DispatchMesh(uint32 numGroupsX, uint32 numGroupsY, uint32 numGroupsZ) {
#if ADVANCED_GPU_FEATURES_ENABLED
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	_commandList->DispatchMesh(numGroupsX, numGroupsY, numGroupsY);
#else
	assert(!"Mesh shaders are not supported with your Windows SDK version.");
#endif
}


void DxCommandList::Raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc) {
	_dynamicDescriptorHeap.CommitStagedDescriptorsForDispatch(this);
	_commandList->DispatchRays(&raytraceDesc);
}

void DxCommandList::QueryTimestamp(uint32 index) {
	_commandList->EndQuery(_timeStampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index);
}

void DxCommandList::Reset() {
	_commandAllocator->Reset();
	ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

	for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
		_descriptorHeaps[i] = nullptr;
	}

	_uploadBuffer.Reset();
	_dynamicDescriptorHeap.Reset();
}
