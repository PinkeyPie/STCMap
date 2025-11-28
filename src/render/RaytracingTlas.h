#pragma once

#include "Raytracing.h"

struct RaytracingInstanceHandle {
    uint32 InstanceIndex;
};

struct RaytracingTlas {
    void Initialize(ERaytracingAsRebuildMode rebuildMode = ERaytracingAsRebuild);

    // Call these each frame to rebuild the structure.
    void Reset();
    RaytracingInstanceHandle Instantiate(RaytracingObjectType type, const trs& transform);
    void Build();


    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> AllInstances;

    ERaytracingAsRebuildMode RebuildMode;

    Ptr<DxBuffer> Scratch;
    Ptr<DxBuffer> Tlas;
};