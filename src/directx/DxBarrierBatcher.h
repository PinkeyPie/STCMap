#pragma once

#include "DxContext.h"

class DxCommandList;

class DxBarrierBatcher {
public:
	DxBarrierBatcher(DxCommandList* cl);
	~DxBarrierBatcher() { Submit(); }

	DxBarrierBatcher& Transition(const DxResource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	DxBarrierBatcher& Transition(const Ptr<DxTexture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	DxBarrierBatcher& Transition(const Ptr<DxBuffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);

	DxBarrierBatcher& TransitionBegin(const DxResource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	DxBarrierBatcher& TransitionBegin(const Ptr<DxTexture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	DxBarrierBatcher& TransitionBegin(const Ptr<DxBuffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);

	DxBarrierBatcher& TransitionEnd(const DxResource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	DxBarrierBatcher& TransitionEnd(const Ptr<DxTexture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	DxBarrierBatcher& TransitionEnd(const Ptr<DxBuffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);

	DxBarrierBatcher& UAV(const DxResource& res);
	DxBarrierBatcher& UAV(const Ptr<DxTexture>& res);
	DxBarrierBatcher& UAV(const Ptr<DxBuffer>& res);

	DxBarrierBatcher& Aliasing(DxResource before, DxResource after);

	void Submit();

	DxCommandList* Cl;
	CD3DX12_RESOURCE_BARRIER Barriers[16];
	uint32 NumBarriers = 0;

};