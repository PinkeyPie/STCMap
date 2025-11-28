#include "RaytracingSpecular.h"
#include "../core/color.h"

#include "raytracing.hlsli"

#define SPECULAR_REFLECTIONS_RS_RESOURCES   0
#define SPECULAR_REFLECTIONS_RS_CAMERA      1
#define SPECULAR_REFLECTIONS_RS_SUN         2
#define SPECULAR_REFLECTIONS_RS_CB          3

void SpecularReflectionsRaytracer::Initialize() {
    const wchar* shaderPath = L"shaders//specular_reflections_rts.hlsl";

    const uint32 numInputResources = sizeof(InputResources) / sizeof(DxCpuDescriptorHandle);
    const uint32 numOutputResources = sizeof(OutputResources) / sizeof(DxCpuDescriptorHandle);

    CD3DX12_DESCRIPTOR_RANGE resourceRanges[] = {
        // Must be input first, then output.
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numInputResources, 0),
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, numOutputResources, 0),
    };

    CD3DX12_ROOT_PARAMETER globalRootParameters[] = {
        RootDescriptorTable(std::size(resourceRanges), resourceRanges),
        RootCBV(0), // Camera.
        RootCBV(1), // Sun.
        RootConstants<RaytracingCb>(2),
    };

    CD3DX12_STATIC_SAMPLER_DESC globalStaticSamplers[] =
    {
       CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR),
       CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
    };

    D3D12_ROOT_SIGNATURE_DESC globalDesc =
    {
        std::size(globalRootParameters), globalRootParameters,
        std::size(globalStaticSamplers), globalStaticSamplers
    };



    CD3DX12_DESCRIPTOR_RANGE hitSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0, 1);
    CD3DX12_ROOT_PARAMETER hitRootParameters[] = {
        RootConstants<PbrMaterialCb>(0, 1),
        RootDescriptorTable(1, &hitSRVRange),
    };

    D3D12_ROOT_SIGNATURE_DESC hitDesc = {
        std::size(hitRootParameters), hitRootParameters
    };


    RaytracingMeshHitGroup radianceHitgroup = { L"radianceClosestHit" };
    RaytracingMeshHitGroup shadowHitgroup = { L"shadowClosestHit" };

    _pipeline = RaytracingPipelineBuilder(shaderPath, 4 * sizeof(float), _maxRecursionDepth, true, false)
        .GlobalRootSignature(globalDesc)
        .RayGen(L"rayGen")
        .HitGroup(L"RADIANCE", L"radianceMiss", radianceHitgroup, hitDesc)
        .HitGroup(L"SHADOW", L"shadowMiss", shadowHitgroup)
        .Finish();

    _numRayTypes = (uint32)_pipeline.ShaderBindingTableDesc.HitGroups.size();

    _bindingTable.Initialize(&_pipeline);
    _descriptorHeap.Inititalize(2048); // TODO.

    AllocateDescriptorHeapSpaceForGlobalResources<InputResources, OutputResources>(_descriptorHeap);
}

RaytracingObjectType SpecularReflectionsRaytracer::DefineObjectType(const Ptr<RaytracingBlas> &blas, const std::vector<Ptr<PbrMaterial> > &materials) {
    uint32 numGeometries = (uint32)blas->Geometries.size();
    for (uint32 i = 0; i < numGeometries; ++i) {
        SubmeshInfo submesh = blas->Geometries[i].Submesh;
        const Ptr<PbrMaterial>& material = materials[i];

        DxCpuDescriptorHandle base = _descriptorHeap.CurrentCPU;

        _descriptorHeap.Push().CreateBufferSRV(blas->Geometries[i].VertexBuffer.get(), { submesh.BaseVertex, submesh.NumVertices });
        _descriptorHeap.Push().CreateRawBufferSRV(blas->Geometries[i].IndexBuffer.get(), { submesh.FirstTriangle * 3, submesh.NumTriangles * 3 });


        uint32 flags = 0;

        if (material->Albedo) {
            _descriptorHeap.Push().Create2DTextureSRV(material->Albedo.get());
            flags |= USE_ALBEDO_TEXTURE;
        }
        else {
            _descriptorHeap.Push().CreateNullTextureSRV();
        }

        if (material->Normal) {
            _descriptorHeap.Push().Create2DTextureSRV(material->Normal.get());
            flags |= USE_NORMAL_TEXTURE;
        }
        else {
            _descriptorHeap.Push().CreateNullTextureSRV();
        }

        if (material->Roughness) {
            _descriptorHeap.Push().Create2DTextureSRV(material->Roughness.get());
            flags |= USE_ROUGHNESS_TEXTURE;
        }
        else {
            _descriptorHeap.Push().CreateNullTextureSRV();
        }

        if (material->Metallic) {
            _descriptorHeap.Push().Create2DTextureSRV(material->Metallic.get());
            flags |= USE_METALLIC_TEXTURE;
        }
        else {
            _descriptorHeap.Push().CreateNullTextureSRV();
        }

        ShaderData hitData[2];
        hitData[0].MaterialCB.Initialize(
            material->AlbedoTint,
            material->Emission.xyz,
            material->RoughnessOverride,
            material->MetallicOverride,
            flags
        );
        hitData[0].Resources = base;

        // Shadow ray does not need anything. Therefore we don't set its properties.

        assert(arraysize(hitData) == _numRayTypes);

        _bindingTable.Push(hitData);
    }


    RaytracingObjectType result = { blas, _instanceContributionToHitGroupIndex };

    _instanceContributionToHitGroupIndex += numGeometries * _numRayTypes;

    return result;
}

void SpecularReflectionsRaytracer::Finish() {
    _bindingTable.Build();
}

void SpecularReflectionsRaytracer::Render(DxCommandList *cl, const RaytracingTlas &tlas, const Ptr<DxTexture> &output, const CommonMaterialInfo &materialInfo) {
    InputResources in;
    in.Tlas = tlas.Tlas->RaytracingSRV;
    in.DepthBuffer = materialInfo.OpaqueDepth->DefaultSRV();
    in.ScreenSpaceNormals = materialInfo.WorldNormals->DefaultSRV();
    in.Irradiance = materialInfo.Irradiance->DefaultSRV();
    in.Environment = materialInfo.Environment->DefaultSRV();
    in.Sky = materialInfo.Sky->DefaultSRV();
    in.Brdf = materialInfo.Brdf->DefaultSRV();

    OutputResources out;
    out.Output = output->DefaultUAV();


    DxGpuDescriptorHandle gpuHandle = CopyGlobalResourcesToDescriptorHeap(in, out);


    // Fill out description.
    D3D12_DISPATCH_RAYS_DESC raytraceDesc;
    FillOutRayTracingRenderDesc(_bindingTable.GetBuffer(), raytraceDesc,
        output->Width(), output->Height(), 1,
        _numRayTypes, _bindingTable.GetNumberOfHitGroups());

    RaytracingCb raytracingCB = { NumBounces, FadeoutDistance, MaxDistance, materialInfo.EnvironmentIntensity, materialInfo.SkyIntensity };


    // Set up pipeline.
    cl->SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, _descriptorHeap.DescriptorHeap.Get());

    cl->SetPipelineState(_pipeline.Pipeline);
    cl->SetComputeRootSignature(_pipeline.RootSignature);

    cl->SetComputeDescriptorTable(SPECULAR_REFLECTIONS_RS_RESOURCES, gpuHandle);
    cl->SetComputeDynamicConstantBuffer(SPECULAR_REFLECTIONS_RS_CAMERA, materialInfo.CameraCBV);
    cl->SetComputeDynamicConstantBuffer(SPECULAR_REFLECTIONS_RS_SUN, materialInfo.SunCBV);
    cl->SetCompute32BitConstants(SPECULAR_REFLECTIONS_RS_CB, raytracingCB);

    cl->Raytrace(raytraceDesc);
}

