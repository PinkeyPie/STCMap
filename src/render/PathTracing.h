#pragma once

#include "Raytracer.h"

#include "material.hlsli"
#include "pbr.hpp"

class PathTracer : public DxRaytracer {
public:
    void Initialize();
    RaytracingObjectType DefineObjectType(const Ptr<RaytracingBlas>& blas, const std::vector<Ptr<PbrMaterial>>& materials);
    void Finish();
    void Render(DxCommandList* cl, const RaytracingTlas& tlas, const Ptr<DxTexture>& output, const CommonMaterialInfo& materialInfo) override;

    const uint32 MaxRecursionDepth = 4;
    const uint32 MaxPayloadSize = 5 * sizeof(float); // Radiance-payload is 1 x float3, 2 x uint.

    // Parameters.
    uint32 NumAveragedFrames = 0;
    uint32 RecursionDepth = MaxRecursionDepth - 1; // [0, maxRecursionDepth - 1]. 0 and 1 don't really make sense. 0 means, that no primary ray is shot. 1 means that no bounce is computed, which leads to 0 light reaching the primary hit.
    uint32 StartRussianRouletteAfter = RecursionDepth; // [0, recursionDepth].

    bool UseThinLensCamera = false;
    float fNumber = 32.f;
    float FocalLength = 1.f;

    bool UseRealMaterials = false;
    bool EnableDirectLighting = false;
    float LightIntensityScale = 1.f;
    float PointLightRadius = 0.1f;

    bool MultipleImportanceSampling = true;

private:
    struct ShaderData // This struct is 32 bytes large, which together with the 32 byte shader identifier is a nice multiple of the required 32-byte-alignment of the binding table entries.
    {
        PbrMaterialCb MaterialCb;
        DxCpuDescriptorHandle Resources; // Vertex buffer, index buffer, PBR textures.
    };

    // Only descriptors in here!
    struct InputResources
    {
        DxCpuDescriptorHandle Tlas;
        DxCpuDescriptorHandle Sky;
    };

    struct OutputResources
    {
        DxCpuDescriptorHandle Output;
    };



    // TODO: The descriptor heap shouldn't be a member of this structure. If we have multiple raytracers which use the same object types, they can share the descriptor heap.
    // For example, this path tracer defines objects with vertex buffer, index buffer and their PBR textures. Other raytracers, which use the same layout (e.g. a specular reflections
    // raytracer) may very well use the same descriptor heap.
    DxPushableResourceDescriptorHeap _descriptorHeap;

    uint32 _instanceContributionToHitGroupIndex = 0;
    uint32 _numRayTypes;

    RaytracingBindingTable<ShaderData> _bindingTable;
};