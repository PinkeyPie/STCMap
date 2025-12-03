#include "TexturePreprocessing.h"
#include "../directx/DxContext.h"
#include "../core/math.h"
#include "../directx/DxBarrierBatcher.h"
#include "../directx/DxCommandList.h"
#include "../directx/DxPipeline.h"

TexturePreprocessor* TexturePreprocessor::_instance = new TexturePreprocessor{};

struct MipmapCb {
    uint32 SrcMipLevel; // Texture level of source mip
    uint32 NumMipLevels; // Shader can generate up to 4 mips at once
    uint32 SrcDimensionFlags; // Flags indicating if the source width/height are even/odd
    uint32 IsSRGB; // Is the source texture in sRGB color space
    vec2 TexelSize; // 1.0 / DstMip1.Dimensions
};

struct EquirectangularToCubemapCb {
    uint32 CubemapSize;   // Size fo cubemap face in pixels
    uint32 FirstMipLevel; // First mip level to process
    uint32 NumMipLevels;  // Number of mip levels to process
    bool IsSRGB;        // Is the source texture in sRGB color space
};

struct CubemapToIrradianceCb {
    uint32 IrradianceMapSize; // Size fo cubemap face in pixels
    float UvzScale;
};

struct PrefilteredEnvironmentCb {
    uint32 CubemapSize;           // Size of cubemap face in pixels at the current mipmal level
    uint32 FirstMip;              // The first mip level to generate
    uint32 NumMipLevels;          // The number of mips to generate
    uint32 TotalNumMipLevels;
};

struct IntegrateBRDFCb {
    uint32 TextureDim;
};

void TexturePreprocessor::Initialize() {
    DxPipelineFactory* factory = DxPipelineFactory::Instance();

    _mipmapPipeline = factory->CreateReloadablePipeline("generate_mips_cs");
    _equirectangularToCubemapPipeline = factory->CreateReloadablePipeline("equirectangular_to_cubemap_cs");
    _cubemapToIrradiancePipeline = factory->CreateReloadablePipeline("cubemap_to_irradiance_cs");
    _prefilteredEnvironmentPipeline = factory->CreateReloadablePipeline("prefilter_environment_cs");
    _integrateBRDFPipeline = factory->CreateReloadablePipeline("integrate_brdf_cs");
}

void TexturePreprocessor::GenerateMipMapsOnGPU(DxCommandList *cl, Ptr<DxTexture> &texture) {
    DxContext& dxContext = DxContext::Instance();

    DxResource resource = texture->Resource;
    if (not resource) {
        return;
    }

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

    uint32 numMips = resourceDesc.MipLevels;
    if (numMips == 1) {
        return;
    }

    if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D or
        resourceDesc.DepthOrArraySize != 1 or
        resourceDesc.SampleDesc.Count > 1) {
        fprintf(stderr, "GenerateMips is only supported for non-multi-sampled 2D Textures.\n");
        return;
    }

    DxResource uavResource = resource;
    DxResource aliasResource; // In case the format of our texture doesn't support UAVs

    if (not texture->SupportsUAV or (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0) {
        D3D12_RESOURCE_DESC aliasDesc = resourceDesc;
        // Placed resources can't be render targets or depth-stencil views.
        aliasDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        aliasDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        // Describe a UAV compatible resource that is used to perform mipmapping of the original texture.
        D3D12_RESOURCE_DESC uavDesc = aliasDesc; // The flags for the UAV description must match that of the alias description
        uavDesc.Format = DxTexture::GetUAVCompatibleFormat(resourceDesc.Format);

        D3D12_RESOURCE_DESC resourceDescs[] = {
            aliasDesc,
            uavDesc
        };

        // Create a heap that is large enough to stare a copy of the original resource
        D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = dxContext.GetDevice()->GetResourceAllocationInfo(0, std::size(resourceDescs), resourceDescs);

        D3D12_HEAP_DESC heapDesc = {};
        heapDesc.SizeInBytes = allocationInfo.SizeInBytes;
        heapDesc.Alignment = allocationInfo.Alignment;
        heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
        heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        DxHeap heap;
        ThrowIfFailed(dxContext.GetDevice()->CreateHeap(&heapDesc, IID_PPV_ARGS(heap.GetAddressOf())));
        dxContext.Retire(heap);

        ThrowIfFailed(dxContext.GetDevice()->CreatePlacedResource(
            heap.Get(),
            0,
            &aliasDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(aliasResource.GetAddressOf())
        ));

        SET_NAME(aliasResource, "Alias resource for mip map generation");
        dxContext.Retire(aliasResource);

        ThrowIfFailed(dxContext.GetDevice()->CreatePlacedResource(
            heap.Get(),
            0,
            &uavDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(uavResource.GetAddressOf())
        ));

        SET_NAME(uavResource, "UAV resource for mip map generation");
        dxContext.Retire(uavResource);

        cl->AliasingBarrier(0, aliasResource);

        // Copy the original resource to the alias resource.
        cl->CopyResource(resource, aliasResource);

        // Add an aliasing barrier for the UAV compatible resource.
        cl->AliasingBarrier(aliasResource, uavResource);
    }

    bool isSRGB = DxTexture::IsSRGBFormat(resourceDesc.Format);
    cl->SetPipelineState(*_mipmapPipeline.Pipeline);
    cl->SetComputeRootSignature(*_mipmapPipeline.RootSignature);

    MipmapCb cb;
    cb.IsSRGB = isSRGB;

    resourceDesc = uavResource->GetDesc();

    DXGI_FORMAT overrideFormat = isSRGB ? DxTexture::GetSRGBFormat(resourceDesc.Format) : resourceDesc.Format;

    Ptr<DxTexture> tmpTexture = MakePtr<DxTexture>();
    tmpTexture->Resource = uavResource;

    uint32 numSrcMipLevels = resourceDesc.MipLevels - 1;
    uint32 numDstMipLevels = resourceDesc.MipLevels - 1;
    uint32 numDescriptors = numSrcMipLevels + numDstMipLevels;

    DxDescriptorRange descriptors = dxContext.FrameDescriptorAllocator().AllocateContiguousDescriptorRange(numDescriptors);

    DxDoubleDescriptorHandle srvOffset;
    for (uint32 i = 0; i < numSrcMipLevels; i++) {
        DxDoubleDescriptorHandle handle = descriptors.PushHandle();
        handle.Create2DTextureSRV(tmpTexture.get(), {i, 1}, overrideFormat);
        if (i == 0) {
            srvOffset = handle;
        }
    }

    DxDoubleDescriptorHandle uavOffset;
    for (uint32 i = 0; i < numDstMipLevels; i++) {
        uint32 mip = i + 1;
        DxDoubleDescriptorHandle handle = descriptors.PushHandle();
        handle.Create2DTextureUAV(tmpTexture.get(), mip);
        if (i == 0) {
            uavOffset = handle;
        }
    }

    cl->SetDescriptorHeap(descriptors);

    for (uint32 srcMip = 0; srcMip < resourceDesc.MipLevels - 1u;) {
        uint64 srcWidth = resourceDesc.Width >> srcMip;
        uint32 srcHeight = resourceDesc.Height >> srcMip;
        uint32 dstWidth = (uint32)(srcWidth >> 1);
        uint32 dstHeight = srcHeight >> 1;

        // 0b00(0): Both width and height are even.
        // 0b01(1): Width is odd, height is even.
        // 0b10(2): Width is even, height is odd.
        // 0b11(3): Both width and height are odd.
        cb.SrcDimensionFlags = (srcHeight & 1) << 1 | (srcWidth & 1);

        DWORD mipCount;

        // The number of times we can half the size of the texture and get
        // exactly a 50% reduction in size.
        // A 1 bit in the width or height indicates an odd dimension.
        // The case where either the width or the height is exactly 1 is handled
        // as a special case (as the dimension does not require reduction).
        _BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));

        // Maximum number of mips to generate is 4
        mipCount = min(4, mipCount + 1);
        // Clamp to total number of mips left over.
        mipCount = (srcMip + mipCount) >= resourceDesc.MipLevels ? resourceDesc.MipLevels - srcMip - 1 : mipCount;

        // Dimensions should not reduce to 0
        // This can happen if the width and height are not the same
        dstWidth = max(1, dstWidth);
        dstHeight = max(1, dstHeight);

        cb.SrcMipLevel = srcMip;
        cb.NumMipLevels = mipCount;
        cb.TexelSize.x = 1.f / (float)dstWidth;
        cb.TexelSize.y = 1.f / (float)dstHeight;

        cl->SetCompute32BitConstants(0, cb);

        cl->SetComputeDescriptorTable(1, CD3DX12_GPU_DESCRIPTOR_HANDLE(srvOffset.GpuHandle, srcMip, descriptors.GetIncrementSize()));
        cl->SetComputeDescriptorTable(2, CD3DX12_GPU_DESCRIPTOR_HANDLE(uavOffset.GpuHandle, srcMip, descriptors.GetIncrementSize()));

        cl->Dispatch(bucketize(dstWidth, 8), bucketize(dstHeight, 8));

        cl->UavBarrier(uavResource);

        srcMip += mipCount;
    }

    if (aliasResource) {
        cl->AliasingBarrier(uavResource, aliasResource);
        // Copy the alias resource back to the original resource
        DxBarrierBatcher(cl)
        .Transition(aliasResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE)
        .Transition(resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        cl->CopyResource(aliasResource, resource);
        cl->TransitionBarrier(resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }
}

Ptr<DxTexture> TexturePreprocessor::EquirectangularToCubemap(DxCommandList *cl, const Ptr<DxTexture> &equirectangular, uint32 resolution, uint32 numMips, DXGI_FORMAT format) {
    assert(equirectangular->Resource);

    DxContext& dxContext = DxContext::Instance();

    CD3DX12_RESOURCE_DESC cubemapDesc(equirectangular->Resource->GetDesc());
    cubemapDesc.Width = cubemapDesc.Height = resolution;
    cubemapDesc.DepthOrArraySize = 6;
    cubemapDesc.MipLevels = numMips;
    if (format != DXGI_FORMAT_UNKNOWN) {
        cubemapDesc.Format = format;
    }
    if (DxTexture::IsUAVCompatibleFormat(cubemapDesc.Format)) {
        cubemapDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    Ptr<DxTexture> cubemap = TextureFactory::Instance()->CreateTexture(cubemapDesc, nullptr, 0);

    cubemapDesc = CD3DX12_RESOURCE_DESC(cubemap->Resource->GetDesc());
    numMips = cubemapDesc.MipLevels;

    DxResource cubemapResource = cubemap->Resource;
    SET_NAME(cubemapResource, "Cubemap");
    DxResource stagingResource = cubemapResource;

    Ptr<DxTexture> stagingTexture = MakePtr<DxTexture>();
    stagingTexture->Resource = cubemap->Resource;

    if ((cubemapDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0) {
        CD3DX12_RESOURCE_DESC stagingDesc = cubemapDesc;
        stagingDesc.Format = DxTexture::GetUAVCompatibleFormat(cubemapDesc.Format);
        stagingDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &stagingDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(stagingResource.GetAddressOf())
        ));

        SET_NAME(stagingResource, "Staging resource for equirectangular to cubemap");
        stagingTexture->Resource = stagingResource;
    }

    cl->SetPipelineState(*_equirectangularToCubemapPipeline.Pipeline);
    cl->SetComputeRootSignature(*_equirectangularToCubemapPipeline.RootSignature);

    bool isSRGB = DxTexture::IsSRGBFormat(cubemapDesc.Format);

    EquirectangularToCubemapCb equirectangularToCubemapCb;
    equirectangularToCubemapCb.IsSRGB = isSRGB;

    DxDescriptorRange descriptors = dxContext.FrameDescriptorAllocator().AllocateContiguousDescriptorRange(numMips + 1);

    DxDoubleDescriptorHandle srvOffset = descriptors.PushHandle();
    srvOffset.Create2DTextureSRV(equirectangular.get());

    cl->SetDescriptorHeap(descriptors);
    cl->SetComputeDescriptorTable(1, srvOffset);

    for (uint32 mipSlice = 0; mipSlice < numMips;) {
        // Maximum number of mips to generate per pass is 5
        uint32 mipCount = min(5, cubemapDesc.MipLevels - mipSlice);

        equirectangularToCubemapCb.FirstMipLevel = mipSlice;
        equirectangularToCubemapCb.CubemapSize = max((uint32)cubemapDesc.Width, cubemapDesc.Height) >> mipSlice;
        equirectangularToCubemapCb.NumMipLevels = mipCount;

        cl->SetCompute32BitConstants(0, equirectangularToCubemapCb);

        for (uint32 mip = 0; mip < mipCount; mip++) {
            DxDoubleDescriptorHandle handle = descriptors.PushHandle();
            handle.Create2DTextureArrayUAV(stagingTexture.get(), mipSlice + mip, DxTexture::GetUAVCompatibleFormat(cubemapDesc.Format));
            if (mip == 0) {
                cl->SetComputeDescriptorTable(2, handle.GpuHandle);
            }
        }

        cl->Dispatch(bucketize(equirectangularToCubemapCb.CubemapSize, 16), bucketize(equirectangularToCubemapCb.CubemapSize, 16), 6);

        mipSlice += mipCount;
    }

    cl->UavBarrier(stagingResource);
    if (stagingResource != cubemapResource) {
        cl->CopyResource(stagingTexture->Resource, cubemap->Resource);
        cl->TransitionBarrier(cubemap->Resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }

    dxContext.Retire(stagingTexture->Resource);

    return cubemap;
}

Ptr<DxTexture> TexturePreprocessor::CubemapToIrradiance(DxCommandList *cl, const Ptr<DxTexture> &environment,
                                                        uint32 resolution, uint32 sourceSlice, float uvzScale) {
    assert(environment->Resource);

    DxContext& dxContext = DxContext::Instance();
    TextureFactory* textureFactory = TextureFactory::Instance();
    CD3DX12_RESOURCE_DESC irradianceDesc(environment->Resource->GetDesc());

    if (DxTexture::IsSRGBFormat(irradianceDesc.Format)) {
        printf("Warning: Irradiance of SRGB-Format!\n");
    }

    irradianceDesc.Width = irradianceDesc.Height = resolution;
    irradianceDesc.DepthOrArraySize = 6;
    irradianceDesc.MipLevels = 1;

    if (DxTexture::IsUAVCompatibleFormat(irradianceDesc.Format)) {
        irradianceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    Ptr<DxTexture> irradiance = textureFactory->CreateTexture(irradianceDesc, 0, 0);

    irradianceDesc = CD3DX12_RESOURCE_DESC(irradiance->Resource->GetDesc());

    DxResource irradianceResource = irradiance->Resource;
    SET_NAME(irradianceResource, "Irradiance");
    DxResource stagingResource = irradianceResource;

    Ptr<DxTexture> stagingTexture =  MakePtr<DxTexture>();
    stagingTexture->Resource = irradiance->Resource;

    if ((irradianceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0) {
        CD3DX12_RESOURCE_DESC stagingDesc = irradianceDesc;
        stagingDesc.Format = DxTexture::GetUAVCompatibleFormat(irradianceDesc.Format);
        stagingDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &stagingDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(stagingResource.GetAddressOf())
        ));
        SET_NAME(stagingResource, "Staging resource for cubemap to irradiance");

        stagingTexture->Resource = stagingResource;
    }

    cl->SetPipelineState(*_cubemapToIrradiancePipeline.Pipeline);
    cl->SetComputeRootSignature(*_cubemapToIrradiancePipeline.RootSignature);

    CubemapToIrradianceCb cubemapToIrradianceCb;

    cubemapToIrradianceCb.IrradianceMapSize = resolution;
    cubemapToIrradianceCb.UvzScale = uvzScale;

    DxDescriptorRange descriptors = dxContext.FrameDescriptorAllocator().AllocateContiguousDescriptorRange(2);
    cl->SetDescriptorHeap(descriptors);

    DxDoubleDescriptorHandle uavOffset = descriptors.PushHandle();
    uavOffset.Create2DTextureArrayUAV(stagingTexture.get(), 0, DxTexture::GetUAVCompatibleFormat(irradianceDesc.Format));

    DxDoubleDescriptorHandle srvOffset = descriptors.PushHandle();
    if (sourceSlice == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
        srvOffset.CreateCubemapSRV(environment.get());
    }
    else {
        srvOffset.CreateCubemapArraySRV(environment.get(), {0, 1}, sourceSlice, 1);
    }

    cl->SetCompute32BitConstants(0, cubemapToIrradianceCb);
    cl->SetComputeDescriptorTable(1, srvOffset);
    cl->SetComputeDescriptorTable(2, uavOffset);

    cl->Dispatch(bucketize(cubemapToIrradianceCb.IrradianceMapSize, 16), bucketize(cubemapToIrradianceCb.IrradianceMapSize, 16), 6);

    cl->UavBarrier(stagingResource);
    if (stagingResource != irradianceResource) {
        cl->CopyResource(stagingTexture->Resource, irradiance->Resource);
        cl->TransitionBarrier(irradiance->Resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }

    dxContext.Retire(stagingTexture->Resource);

    return irradiance;
}

Ptr<DxTexture> TexturePreprocessor::PrefilterEnvironment(DxCommandList *cl, const Ptr<DxTexture> &environment, uint32 resolution) {
    assert(environment->Resource);

    DxContext& dxContext = DxContext::Instance();
    TextureFactory* textureFactory = TextureFactory::Instance();
    CD3DX12_RESOURCE_DESC prefilteredCreationDesc(environment->Resource->GetDesc());
    prefilteredCreationDesc.Width = prefilteredCreationDesc.Height = resolution;
    prefilteredCreationDesc.DepthOrArraySize = 6;
    prefilteredCreationDesc.MipLevels = 0;

    if (DxTexture::IsUAVCompatibleFormat(prefilteredCreationDesc.Format)) {
        prefilteredCreationDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    Ptr<DxTexture> prefiltered = textureFactory->CreateTexture(prefilteredCreationDesc, nullptr, 0);

    auto prefilteredDesc = CD3DX12_RESOURCE_DESC(prefiltered->Resource->GetDesc());

    DxResource prefilteredResource = prefiltered->Resource;
    SET_NAME(prefilteredResource, "Prefiltered");

    DxResource stagingResource = prefilteredResource;

    Ptr<DxTexture> stagingTexture = MakePtr<DxTexture>();
    stagingTexture->Resource = prefiltered->Resource;

    if ((prefilteredDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0) {
        CD3DX12_RESOURCE_DESC stagingDesc = prefilteredDesc;
        stagingDesc.Format = DxTexture::GetUAVCompatibleFormat(prefilteredDesc.Format);
        stagingDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
            &properties,
            D3D12_HEAP_FLAG_NONE,
            &stagingDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&stagingResource)
        ));

        stagingTexture->Resource = stagingResource;

        SET_NAME(stagingTexture->Resource, "Staging");
    }

    cl->SetPipelineState(*_prefilteredEnvironmentPipeline.Pipeline);
    cl->SetComputeRootSignature(*_prefilteredEnvironmentPipeline.RootSignature);

    PrefilteredEnvironmentCb prefilteredEnvironmentCb;
    prefilteredEnvironmentCb.TotalNumMipLevels = prefilteredDesc.MipLevels;

    DxDescriptorRange descriptors = dxContext.FrameDescriptorAllocator().AllocateContiguousDescriptorRange(prefilteredDesc.MipLevels + 1);
    cl->SetDescriptorHeap(descriptors);

    DxDoubleDescriptorHandle srvOffset = descriptors.PushHandle();
    srvOffset.CreateCubemapSRV(environment.get());
    cl->SetComputeDescriptorTable(1, srvOffset);

    for (uint32 mipSlice = 0; mipSlice < prefilteredDesc.MipLevels; ) {
        // Maximum number of mips to generate per pass is 5
        uint32 numMips = min(5, prefilteredDesc.MipLevels - mipSlice);

        prefilteredEnvironmentCb.FirstMip = mipSlice;
        prefilteredEnvironmentCb.CubemapSize = max((uint32)prefilteredDesc.Width, prefilteredDesc.Height) >> mipSlice;
        prefilteredEnvironmentCb.NumMipLevels = numMips;

        cl->SetCompute32BitConstants(0, prefilteredEnvironmentCb);

        for (uint32 mip = 0; mip < numMips; mip++) {
            DxDoubleDescriptorHandle handle = descriptors.PushHandle();
            handle.Create2DTextureArrayUAV(stagingTexture.get(), mipSlice + mip, DxTexture::GetUAVCompatibleFormat(prefilteredDesc.Format));
            if (mip == 0) {
                cl->SetComputeDescriptorTable(2, handle);
            }
        }

        cl->Dispatch(bucketize(prefilteredEnvironmentCb.CubemapSize, 16), bucketize(prefilteredEnvironmentCb.CubemapSize, 16), 6);

        mipSlice += numMips;
    }

    cl->UavBarrier(stagingResource);
    if (stagingResource != prefilteredResource) {
        cl->CopyResource(stagingTexture->Resource, prefiltered->Resource);
        cl->TransitionBarrier(prefiltered->Resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }

    return prefiltered;
}

Ptr<DxTexture> TexturePreprocessor::IntegrateBRDF(DxCommandList *cl, uint32 resolution) {
    DxContext& dxContext = DxContext::Instance();
    TextureFactory* textureFactory = TextureFactory::Instance();
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R16G16_FLOAT,
        resolution, resolution, 1, 1
    );

    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    Ptr<DxTexture> brdf = textureFactory->CreateTexture(desc, nullptr, 0);
    desc = CD3DX12_RESOURCE_DESC(brdf->Resource->GetDesc());

    // TODO: Technically R16G16 is not guaranteed to be UAV compatible.
    // If we ever run on hardware, which does not support this, we need to find a solution.
    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/typed-unordered-access-view-loads

    DxResource brdfResource = brdf->Resource;
    SET_NAME(brdfResource, "BRDF");

    cl->SetPipelineState(*_integrateBRDFPipeline.Pipeline);
    cl->SetComputeRootSignature(*_integrateBRDFPipeline.RootSignature);

    IntegrateBRDFCb integrateBrdfCb;
    integrateBrdfCb.TextureDim = resolution;

    cl->SetCompute32BitConstants(0, integrateBrdfCb);

    cl->SetDescriptorHeapUAV(1, 0, brdf);
    cl->ResetToDynamicDescriptorHeap();

    cl->Dispatch(bucketize(resolution, 16), bucketize(resolution, 16), 1);

    cl->UavBarrier(brdf);

    return brdf;
}
