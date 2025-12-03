#include "DxBarrierBatcher.h"
#include "DxCommandList.h"

DxBarrierBatcher::DxBarrierBatcher(DxCommandList* cl) {
	this->Cl = cl;
}

DxBarrierBatcher& DxBarrierBatcher::Transition(const DxResource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	if (NumBarriers == std::size(Barriers)) {
		Submit();
	}

	if (from != to) {
		Barriers[NumBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource);
	}

	return *this;
}

DxBarrierBatcher &DxBarrierBatcher::Transition(const Ptr<DxTexture> &res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	if (!res) {
		return *this;
	}

	return Transition(res->Resource, from, to, subresource);
}

DxBarrierBatcher &DxBarrierBatcher::Transition(const Ptr<DxBuffer> &res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
	if (!res) {
		return *this;
	}

	return Transition(res->Resource, from, to);
}

DxBarrierBatcher &DxBarrierBatcher::TransitionBegin(const DxResource &res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	if (NumBarriers == std::size(Barriers)) {
		Submit();
	}

	if (from != to) {
		Barriers[NumBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);
	}

	return *this;
}

DxBarrierBatcher &DxBarrierBatcher::TransitionBegin(const Ptr<DxTexture> &res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	if (!res) {
		return *this;
	}

	return TransitionBegin(res->Resource, from, to);
}

DxBarrierBatcher &DxBarrierBatcher::TransitionBegin(const Ptr<DxBuffer> &res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
	if (!res) {
		return *this;
	}

	return TransitionBegin(res->Resource, from, to);
}

DxBarrierBatcher& DxBarrierBatcher::TransitionEnd(const DxResource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	if (NumBarriers == std::size(Barriers)) {
		Submit();
	}

	if (from != to) {
		Barriers[NumBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY);
	}
	return *this;
}

DxBarrierBatcher& DxBarrierBatcher::TransitionEnd(const Ptr<DxTexture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource) {
	if (!res) {
		return *this;
	}

	return TransitionEnd(res->Resource, from, to, subresource);
}

DxBarrierBatcher& DxBarrierBatcher::TransitionEnd(const Ptr<DxBuffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
	if (!res) {
		return *this;
	}

	return TransitionEnd(res->Resource, from, to);
}

DxBarrierBatcher& DxBarrierBatcher::UAV(const DxResource& resource) {
	if (NumBarriers == std::size(Barriers)) {
		Submit();
	}

	Barriers[NumBarriers++] = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	return *this;
}

DxBarrierBatcher &DxBarrierBatcher::UAV(const Ptr<DxTexture> &res) {
	if (!res) {
		return *this;
	}

	return UAV(res->Resource);
}

DxBarrierBatcher &DxBarrierBatcher::UAV(const Ptr<DxBuffer> &res) {
	if (!res) {
		return *this;
	}

	return UAV(res->Resource);
}

DxBarrierBatcher& DxBarrierBatcher::Aliasing(DxResource before, DxResource after) {
	if (NumBarriers == std::size(Barriers)) {
		Submit();
	}

	Barriers[NumBarriers++] = CD3DX12_RESOURCE_BARRIER::Aliasing(before.Get(), after.Get());
	return *this;
}

void DxBarrierBatcher::Submit() {
	if (NumBarriers) {
		Cl->Barriers(Barriers, NumBarriers);
		NumBarriers = 0;
	}
}

