#include "RaytracingTlas.h"

#include "../directx/DxCommandList.h"
#include "../directx/DxBarrierBatcher.h"

void RaytracingTlas::Initialize(ERaytracingAsRebuildMode rebuildMode) {
    RebuildMode = rebuildMode;
    AllInstances.reserve(4096); // Todo
}

void RaytracingTlas::Reset() {
    AllInstances.clear();
}

RaytracingInstanceHandle RaytracingTlas::Instantiate(RaytracingObjectType type, const trs &transform) {
    uint32 result = (uint32)AllInstances.size();
    D3D12_RAYTRACING_INSTANCE_DESC& instance = AllInstances.emplace_back();

    instance.Flags = 0;// D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    instance.InstanceContributionToHitGroupIndex = type.InstanceContributionToHitGroupIndex;

    mat4 m = transpose(trsToMat4(transform));
    memcpy(instance.Transform, &m, sizeof(instance.Transform));
    instance.AccelerationStructure = type.Blas->Blas->GpuVirtualAddress;
    instance.InstanceMask = 0xFF;
    instance.InstanceID = 0; // This value will be exposed to the shader via InstanceID().

    return { result };
}

void RaytracingTlas::Build() {
    uint32 totalNumInstances = (uint32)AllInstances.size();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = totalNumInstances;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    if (RebuildMode == ERaytracingAsRefit) {
        inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    }

    DxContext& dxContext = DxContext::Instance();
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    dxContext.GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
    info.ScratchDataSizeInBytes = AlignTo(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    info.ResultDataMaxSizeInBytes = AlignTo(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    bool fromScratch = false;

    // Allocate.
    if (!Tlas || Tlas->TotalSize < info.ResultDataMaxSizeInBytes) {
        if (Tlas) {
            Tlas->Resize((uint32)info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        }
        else {
            Tlas = DxBuffer::CreateRaytracingTLASBuffer((uint32)info.ResultDataMaxSizeInBytes);
            SET_NAME(Tlas->Resource, "TLAS Result");
        }

        fromScratch = true;
    }

    if (!Scratch || Scratch->TotalSize < info.ScratchDataSizeInBytes) {
        if (Scratch) {
            Scratch->Resize((uint32)info.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        else {
            Scratch = DxBuffer::Create(1, (uint32)info.ScratchDataSizeInBytes, 0, true, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            SET_NAME(Scratch->Resource, "TLAS Scratch");
        }
    }

    DxCommandList* cl = dxContext.GetFreeComputeCommandList(true);
    DxDynamicConstantBuffer gpuInstances = dxContext.UploadDynamicConstantBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalNumInstances, AllInstances.data());

    inputs.InstanceDescs = gpuInstances.GpuPtr;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.DestAccelerationStructureData = Tlas->GpuVirtualAddress;
    asDesc.ScratchAccelerationStructureData = Scratch->GpuVirtualAddress;

    if (!fromScratch) {
        DxBarrierBatcher(cl).UAV(Tlas).UAV(Scratch);

        if (RebuildMode == ERaytracingAsRefit) {
            asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            asDesc.SourceAccelerationStructureData = Tlas->GpuVirtualAddress;
        }
    }


    cl->CommandList()->BuildRaytracingAccelerationStructure(&asDesc, 0, 0);
    cl->UavBarrier(Tlas);

    dxContext.ExecuteCommandList(cl);
}


