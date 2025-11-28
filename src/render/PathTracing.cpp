#include "PathTracing.h"
#include "../core/color.h"

#include "raytracing.hlsli"

#define PATH_TRACING_RS_RESOURCES   0
#define PATH_TRACING_RS_CAMERA      1
#define PATH_TRACING_RS_CB          2

void PathTracer::Initialize() {
    const wchar *shaderPath = L"shaders//path_tracing_rts.hlsl";


    const uint32 numInputResources = sizeof(InputResources) / sizeof(DxCpuDescriptorHandle);
    const uint32 numOutputResources = sizeof(OutputResources) / sizeof(DxCpuDescriptorHandle);

    CD3DX12_DESCRIPTOR_RANGE resourceRanges[] =
    {
        // Must be input first, then output.
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numInputResources, 0),
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, numOutputResources, 0),
    };

    CD3DX12_ROOT_PARAMETER globalRootParameters[] =
    {
        RootDescriptorTable(arraysize(resourceRanges), resourceRanges),
        RootCBV(0), // Camera.
        RootConstants<PathTracingCb>(1),
    };

    CD3DX12_STATIC_SAMPLER_DESC globalStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

    D3D12_ROOT_SIGNATURE_DESC globalDesc =
    {
        arraysize(globalRootParameters), globalRootParameters,
        1, &globalStaticSampler
    };


    // 6 Elements: Vertex buffer, index buffer, albedo texture, normal map, roughness texture, metallic texture.
    CD3DX12_DESCRIPTOR_RANGE hitSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0, 1);
    CD3DX12_ROOT_PARAMETER hitRootParameters[] =
    {
        RootConstants<PbrMaterialCb>(0, 1),
        RootDescriptorTable(1, &hitSRVRange),
    };

    D3D12_ROOT_SIGNATURE_DESC hitDesc =
    {
        arraysize(hitRootParameters), hitRootParameters
    };


    RaytracingMeshHitGroup radianceHitgroup = {L"radianceClosestHit"};
    RaytracingMeshHitGroup shadowHitgroup = {};

    _pipeline = RaytracingPipelineBuilder(shaderPath, MaxPayloadSize, MaxRecursionDepth, true, false)
    .GlobalRootSignature(globalDesc)
    .RayGen(L"rayGen")
    .HitGroup(L"RADIANCE", L"radianceMiss", radianceHitgroup, hitDesc)
    .HitGroup(L"SHADOW", L"shadowMiss", shadowHitgroup)
    .Finish();

    _numRayTypes = (uint32) _pipeline.ShaderBindingTableDesc.HitGroups.size();

    _bindingTable.Initialize(&_pipeline);
    _descriptorHeap.Inititalize(2048); // TODO.

    AllocateDescriptorHeapSpaceForGlobalResources<InputResources, OutputResources>(_descriptorHeap);
}

RaytracingObjectType PathTracer::DefineObjectType(const Ptr<RaytracingBlas> &blas,
                                                  const std::vector<Ptr<PbrMaterial> > &materials) {
    uint32 numGeometries = (uint32) blas->Geometries.size();

    ShaderData *hitData = (ShaderData *) alloca(sizeof(ShaderData) * _numRayTypes);

    for (uint32 i = 0; i < numGeometries; ++i) {
        assert(blas->Geometries[i].Type == ERaytracingMeshGeometry); // For now we only support meshes, not procedurals.

        SubmeshInfo submesh = blas->Geometries[i].Submesh;
        const Ptr<PbrMaterial> &material = materials[i];

        DxCpuDescriptorHandle base = _descriptorHeap.CurrentCPU;

        _descriptorHeap.Push().CreateBufferSRV(blas->Geometries[i].VertexBuffer.get(),
                                               {submesh.BaseVertex, submesh.NumVertices});
        _descriptorHeap.Push().CreateRawBufferSRV(blas->Geometries[i].IndexBuffer.get(),
                                                  {submesh.FirstTriangle * 3, submesh.NumTriangles * 3});


        uint32 flags = 0;

        if (material->Albedo) {
            _descriptorHeap.Push().Create2DTextureSRV(material->Albedo.get());
            flags |= USE_ALBEDO_TEXTURE;
        } else {
            _descriptorHeap.Push().CreateNullTextureSRV();
        }

        if (material->Normal) {
            _descriptorHeap.Push().Create2DTextureSRV(material->Normal.get());
            flags |= USE_NORMAL_TEXTURE;
        } else {
            _descriptorHeap.Push().CreateNullTextureSRV();
        }

        if (material->Roughness) {
            _descriptorHeap.Push().Create2DTextureSRV(material->Roughness.get());
            flags |= USE_ROUGHNESS_TEXTURE;
        } else {
            _descriptorHeap.Push().CreateNullTextureSRV();
        }

        if (material->Metallic) {
            _descriptorHeap.Push().Create2DTextureSRV(material->Metallic.get());
            flags |= USE_METALLIC_TEXTURE;
        } else {
            _descriptorHeap.Push().CreateNullTextureSRV();
        }

        hitData[0].MaterialCb.Initialize(
            material->AlbedoTint,
            material->Emission.xyz,
            material->RoughnessOverride,
            material->MetallicOverride,
            flags
        );
        hitData[0].Resources = base;

        // The other shader types don't need any data, so we don't set it here.

        _bindingTable.Push(hitData);
    }


    RaytracingObjectType result = {blas, _instanceContributionToHitGroupIndex};

    _instanceContributionToHitGroupIndex += numGeometries * _numRayTypes;

    return result;
}

void PathTracer::Finish() {
    _bindingTable.Build();
}

void PathTracer::Render(DxCommandList *cl, const RaytracingTlas &tlas, const Ptr<DxTexture> &output, const CommonMaterialInfo &materialInfo) {
    InputResources in;
    in.Tlas = tlas.Tlas->RaytracingSRV;
    in.Sky = materialInfo.Sky->DefaultSRV();

    OutputResources out;
    out.Output = output->DefaultUAV();


    DxGpuDescriptorHandle gpuHandle = CopyGlobalResourcesToDescriptorHeap(in, out);


    // Fill out description.
    D3D12_DISPATCH_RAYS_DESC raytraceDesc;
    FillOutRayTracingRenderDesc(_bindingTable.GetBuffer(), raytraceDesc,
                                output->Width(), output->Height(), 1,
                                _numRayTypes, _bindingTable.GetNumberOfHitGroups());


    // Set up pipeline.
    cl->SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, _descriptorHeap.DescriptorHeap.Get());

    cl->SetPipelineState(_pipeline.Pipeline);
    cl->SetComputeRootSignature(_pipeline.RootSignature);

    uint32 depth = Min(RecursionDepth, MaxRecursionDepth - 1);

    DxContext &dxContext = DxContext::Instance();
    cl->SetComputeDescriptorTable(PATH_TRACING_RS_RESOURCES, gpuHandle);
    cl->SetComputeDynamicConstantBuffer(PATH_TRACING_RS_CAMERA, materialInfo.CameraCBV);
    cl->SetCompute32BitConstants(PATH_TRACING_RS_CB,
                                 PathTracingCb
                                 {
                                     (uint32) dxContext.FrameId(),
                                     NumAveragedFrames,
                                     depth,
                                     clamp(StartRussianRouletteAfter, 0u, depth),
                                     (uint32) UseThinLensCamera,
                                     FocalLength,
                                     FocalLength / (2.f * fNumber),
                                     (uint32) UseRealMaterials,
                                     (uint32) EnableDirectLighting,
                                     LightIntensityScale,
                                     PointLightRadius,
                                     (uint32) MultipleImportanceSampling,
                                 });

    cl->Raytrace(raytraceDesc);

    ++NumAveragedFrames;
}
