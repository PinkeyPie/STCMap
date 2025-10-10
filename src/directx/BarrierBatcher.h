#pragma once

#include "DxContext.h"

class DxCommandList;

class BarrierBatcher {
public:
	BarrierBatcher(DxCommandList* cl);
	~BarrierBatcher() { Submit(); }

	BarrierBatcher& Transition(DxResource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	BarrierBatcher& UAV(DxResource& resource);
	BarrierBatcher& Aliasing(DxResource before, DxResource after);

	void Submit();

	DxCommandList* Cl;
	CD3DX12_RESOURCE_BARRIER Barriers[16];
	uint32 NumBarriers = 0;

};