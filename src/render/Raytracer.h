#pragma once

#include "Raytracing.h"
#include "RaytracingTlas.h"
#include "RaytracingBindingTable.h"
#include "../directx/DxDescriptorAllocation.h"

#include "../directx/DxCommandList.h"

#include "material.h"

class DxRaytracer {
public:
	virtual void Render(DxCommandList* cl, const RaytracingTlas& tlas,
		const Ptr<DxTexture>& output,
		const CommonMaterialInfo& materialInfo) = 0;

protected:
	void FillOutRayTracingRenderDesc(const Ptr<DxBuffer>& bindingTableBuffer,
		D3D12_DISPATCH_RAYS_DESC& raytraceDesc,
		uint32 renderWidth, uint32 renderHeight, uint32 renderDepth,
		uint32 numRayTypes, uint32 numHitGroups);

	template <typename InputResources, typename OutputResources>
	DxGpuDescriptorHandle CopyGlobalResourcesToDescriptorHeap(const InputResources& in, const OutputResources& out);

	template <typename InputResources, typename OutputResources>
	void AllocateDescriptorHeapSpaceForGlobalResources(DxPushableResourceDescriptorHeap& descriptorHeap);

	DxRaytracingPipeline _pipeline;


	DxCpuDescriptorHandle _resourceCPUBase[NUM_BUFFERED_FRAMES];
	DxGpuDescriptorHandle _resourceGPUBase[NUM_BUFFERED_FRAMES];
};

template<typename InputResources, typename OutputResources>
inline DxGpuDescriptorHandle DxRaytracer::CopyGlobalResourcesToDescriptorHeap(const InputResources& in, const OutputResources& out) {
	DxContext& dxContext = DxContext::Instance();
	DxCpuDescriptorHandle cpuHandle = _resourceCPUBase[dxContext.BufferedFrameId()];
	DxGpuDescriptorHandle gpuHandle = _resourceGPUBase[dxContext.BufferedFrameId()];

	const uint32 numInputResources = sizeof(InputResources) / sizeof(DxCpuDescriptorHandle);
	const uint32 numOutputResources = sizeof(OutputResources) / sizeof(DxCpuDescriptorHandle);
	const uint32 totalNumResources = numInputResources + numOutputResources;

	D3D12_CPU_DESCRIPTOR_HANDLE handles[totalNumResources];
	memcpy(handles, &in, sizeof(InputResources));
	memcpy((D3D12_CPU_DESCRIPTOR_HANDLE*)handles + numInputResources, &out, sizeof(OutputResources));

	dxContext.GetDevice()->CopyDescriptors(
		1, (D3D12_CPU_DESCRIPTOR_HANDLE*)&cpuHandle, &totalNumResources,
		totalNumResources, handles, nullptr,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	return gpuHandle;
}

template<typename InputResources, typename OutputResources>
inline void DxRaytracer::AllocateDescriptorHeapSpaceForGlobalResources(DxPushableResourceDescriptorHeap& descriptorHeap)
{
	const uint32 numInputResources = sizeof(InputResources) / sizeof(DxCpuDescriptorHandle);
	const uint32 numOutputResources = sizeof(OutputResources) / sizeof(DxCpuDescriptorHandle);
	const uint32 totalNumResources = numInputResources + numOutputResources;

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		_resourceCPUBase[i] = descriptorHeap.CurrentCPU;
		_resourceGPUBase[i] = descriptorHeap.CurrentGpu;

		for (uint32 j = 0; j < totalNumResources; ++j)
		{
			descriptorHeap.Push();
		}
	}
}