#include "BarrierBatcher.h"
#include "DxCommandList.h"

BarrierBatcher::BarrierBatcher(DxCommandList* cl) {
	this->Cl = cl;
}

BarrierBatcher& BarrierBatcher::Transition(DxResource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	if (NumBarriers == std::size(Barriers)) {
		Submit();
	}

	Barriers[NumBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource);
	return *this;
}

BarrierBatcher& BarrierBatcher::UAV(DxResource& resource) {
	if (NumBarriers == std::size(Barriers)) {
		Submit();
	}

	Barriers[NumBarriers++] = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	return *this;
}

BarrierBatcher& BarrierBatcher::Aliasing(DxResource before, DxResource after) {
	if (NumBarriers == std::size(Barriers)) {
		Submit();
	}

	Barriers[NumBarriers++] = CD3DX12_RESOURCE_BARRIER::Aliasing(before.Get(), after.Get());
	return *this;
}

void BarrierBatcher::Submit() {
	if (NumBarriers) {
		Cl->Barriers(Barriers, NumBarriers);
		NumBarriers = 0;
	}
}

