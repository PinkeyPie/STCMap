#pragma once

#include "Raytracer.h"

#include "material.hlsli"
#include "pbr.hpp"

class SpecularReflectionsRaytracer : public DxRaytracer {
public:
    void Initialize();
    RaytracingObjectType DefineObjectType(const Ptr<RaytracingBlas>& blas, const std::vector<Ptr<PbrMaterial>>& materials);

    void Finish();

    void Render(DxCommandList* cl, const RaytracingTlas& tlas,
        const Ptr<DxTexture>& output,
        const CommonMaterialInfo& materialInfo) override;


    // Parameters. Can be changed in each frame.
    uint32 NumBounces = 1;
    float FadeoutDistance = 80.f;
    float MaxDistance = 100.f;

private:
    struct ShaderData {
        // Only set in radiance hit.
        PbrMaterialCb MaterialCB;
        DxCpuDescriptorHandle Resources; // Vertex buffer, index buffer, pbr textures.
    };

    // Only descriptors in here!
    struct InputResources {
        DxCpuDescriptorHandle Tlas;
        DxCpuDescriptorHandle DepthBuffer;
        DxCpuDescriptorHandle ScreenSpaceNormals;
        DxCpuDescriptorHandle Irradiance;
        DxCpuDescriptorHandle Environment;
        DxCpuDescriptorHandle Sky;
        DxCpuDescriptorHandle Brdf;
    };

    struct OutputResources {
        DxCpuDescriptorHandle Output;
    };

    DxPushableResourceDescriptorHeap _descriptorHeap;

    uint32 _instanceContributionToHitGroupIndex = 0;
    uint32 _numRayTypes;

    const uint32 _maxRecursionDepth = 4;

    RaytracingBindingTable<ShaderData> _bindingTable;
};
