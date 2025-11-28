#include "Raytracer.h"

void DxRaytracer::FillOutRayTracingRenderDesc(const Ptr<DxBuffer> &bindingTableBuffer, D3D12_DISPATCH_RAYS_DESC &raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 renderDepth, uint32 numRayTypes, uint32 numHitGroups) {
    raytraceDesc.Width = renderWidth;
    raytraceDesc.Height = renderHeight;
    raytraceDesc.Depth = renderDepth;

    uint32 numHitShaders = numHitGroups * numRayTypes;

    // Pointer to the entry point of the ray-generation shader.
    raytraceDesc.RayGenerationShaderRecord.StartAddress = bindingTableBuffer->GpuVirtualAddress + _pipeline.ShaderBindingTableDesc.RayGenOffset;
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = _pipeline.ShaderBindingTableDesc.EntrySize;

    // Pointer to the entry point(s) of the miss shader.
    raytraceDesc.MissShaderTable.StartAddress = bindingTableBuffer->GpuVirtualAddress + _pipeline.ShaderBindingTableDesc.MissOffset;
    raytraceDesc.MissShaderTable.StrideInBytes = _pipeline.ShaderBindingTableDesc.EntrySize;
    raytraceDesc.MissShaderTable.SizeInBytes = _pipeline.ShaderBindingTableDesc.EntrySize * numRayTypes;

    // Pointer to the entry point(s) of the hit shader.
    raytraceDesc.HitGroupTable.StartAddress = bindingTableBuffer->GpuVirtualAddress + _pipeline.ShaderBindingTableDesc.HitOffset;
    raytraceDesc.HitGroupTable.StrideInBytes = _pipeline.ShaderBindingTableDesc.EntrySize;
    raytraceDesc.HitGroupTable.SizeInBytes = _pipeline.ShaderBindingTableDesc.EntrySize * numHitShaders;

    raytraceDesc.CallableShaderTable = {};
}
