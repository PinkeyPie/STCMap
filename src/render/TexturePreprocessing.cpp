#include "TexturePreprocessing.h"
#include "../directx/DxContext.h"
#include "../core/math.h"
#include "../directx/BarrierBatcher.h"
#include "../directx/DxCommandList.h"
#include "../directx/DxPipeline.h"
#include "../directx/DxRenderPrimitives.h"

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
    _prefilteredEnvironmentPipeline = factory->CreateReloadablePipeline("prefiltered_environment_cs");
    _integrateBRDFPipeline = factory->CreateReloadablePipeline("integrate_brdf_cs");
}

void TexturePreprocessor::GenerateMipMapsOnGPU(DxCommandList *cl, DxTexture &texture) {
    DxContext& dxContext = DxContext::Instance();

    DxResource resource = texture.Resource;
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

    if (not texture.FormatSupportsUAV() or (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0) {
        D3D12_RESOURCE_DESC aliasDesc = resourceDesc;
        // Placed resources can't be render targets or depth-stencil views.
        aliasDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        aliasDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        // Describe a UAV compatible resource that is used to perform mipmapping of the original texture.
        D3D12_RESOURCE_DESC uavDesc = aliasDesc; // The flags for the UAV description must match that of the alias description
        uavDesc.Format = GetUAVCompatibleFormat(resourceDesc.Format);

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
        dxContext.RetireObject(heap);

        ThrowIfFailed(dxContext.GetDevice()->CreatePlacedResource(
            heap.Get(),
            0,
            &aliasDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(aliasResource.GetAddressOf())
        ));

        dxContext.RetireObject(aliasResource);

        ThrowIfFailed(dxContext.GetDevice()->CreatePlacedResource(
            heap.Get(),
            0,
            &uavDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(uavResource.GetAddressOf())
        ));

        dxContext.RetireObject(uavResource);

        cl->AliasingBarrier(0, aliasResource);

        // Copy the original resource to the alias resource.
        cl->CopyResource(resource, aliasResource);

        // Add an aliasing barrier for the UAV compatible resource.
        cl->AliasingBarrier(aliasResource, uavResource);
    }

    bool isSRGB = IsSRGBFormat(resourceDesc.Format);
    cl->SetPipelineState(*_mipmapPipeline.Pipeline);
    cl->SetComputeRootSignature(*_mipmapPipeline.RootSignature);

    MipmapCb cb;
    cb.IsSRGB = isSRGB;

    resourceDesc = uavResource->GetDesc();

    DXGI_FORMAT overrideFormat = isSRGB ? GetSRGBFormat(resourceDesc.Format) : resourceDesc.Format;

    DxTexture tmpTexture = { uavResource };

    uint32 numSrcMipLevels = resourceDesc.MipLevels - 1;
    uint32 numDstMipLevels = resourceDesc.MipLevels - 1;
    uint32 numDescriptors = numSrcMipLevels + numDstMipLevels;

    DxDescriptorRange descriptors = dxContext.AllocateContiguousDescriptorRange(numDescriptors);

    DxDescriptorHandle srvOffset;
    for (uint32 i = 0; i < numSrcMipLevels; i++) {
        DxDescriptorHandle h = descriptors.Push2DTextureSRV(tmpTexture, {i, 1}, overrideFormat);
        if (i == 0) {
            srvOffset = h;
        }
    }

    DxDescriptorHandle uavOffset;
    for (uint32 i = 0; i < numDstMipLevels; i++) {
        uint32 mip = i + 1;
        DxDescriptorHandle h = descriptors.Push2DTextureUAV(tmpTexture, mip);
        if (i == 0) {
            uavOffset = h;
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
        BarrierBatcher(cl)
        .Transition(aliasResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE)
        .Transition(resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        cl->CopyResource(aliasResource, resource);
        cl->TransitionBarrier(resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }
}

DxTexture TexturePreprocessor::EquirectangularToCubemap(DxCommandList *cl, DxTexture &equirectangular, uint32 resolution, uint32 numMips, DXGI_FORMAT format) {
    assert(equirectangular.Resource);

    DxContext& dxContext = DxContext::Instance();

    CD3DX12_RESOURCE_DESC cubemapDesc(equirectangular.Resource->GetDesc());
    cubemapDesc.Width = cubemapDesc.Height = resolution;
    cubemapDesc.DepthOrArraySize = 6;
    cubemapDesc.MipLevels = numMips;
    if (format != DXGI_FORMAT_UNKNOWN) {
        cubemapDesc.Format = format;
    }

    DxTexture cubemap = DxTexture::Create(cubemapDesc, nullptr, 0);

    cubemapDesc = CD3DX12_RESOURCE_DESC(cubemap.Resource->GetDesc());
    numMips = cubemapDesc.MipLevels;

    DxResource cubemapResource = cubemap.Resource;
    DxResource stagingResource = cubemapResource;

    DxTexture stagingTexture;

    if ((cubemapDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0) {
        CD3DX12_RESOURCE_DESC stagingDesc = cubemapDesc;
        stagingDesc.Format = GetUAVCompatibleFormat(cubemapDesc.Format);
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

        stagingTexture.Resource = stagingResource;
    }

    cl->SetPipelineState(*_equirectangularToCubemapPipeline.Pipeline);
    cl->SetComputeRootSignature(*_equirectangularToCubemapPipeline.RootSignature);

    bool isSRGB = IsSRGBFormat(cubemapDesc.Format);

    EquirectangularToCubemapCb equirectangularToCubemapCb;
    equirectangularToCubemapCb.IsSRGB = isSRGB;

    DxDescriptorRange descriptors = dxContext.AllocateContiguousDescriptorRange(numMips + 1);

    DxDescriptorHandle srvOffset = descriptors.Push2DTextureSRV(equirectangular);

    cl->SetDescriptorHeap(descriptors);
    cl->SetComputeDescriptorTable(1, srvOffset);

    for (uint32 mipSlice = 0; mipSlice < numMips;) {
        // Maximum number of mips to generate per pass is 5
        uint32 numMips = min(5, cubemapDesc.MipLevels - mipSlice);

        equirectangularToCubemapCb.FirstMipLevel = mipSlice;
        equirectangularToCubemapCb.CubemapSize = max((uint32)cubemapDesc.Width, cubemapDesc.Height) >> mipSlice;
        equirectangularToCubemapCb.NumMipLevels = numMips;

        cl->SetCompute32BitConstants(0, equirectangularToCubemapCb);

        for (uint32 mip = 0; mip < numMips; mip++) {
            DxDescriptorHandle h = descriptors.Push2DTextureArrayUAV(stagingTexture, mipSlice + mip, GetUAVCompatibleFormat(cubemapDesc.Format));
            if (mip == 0) {
                cl->SetComputeDescriptorTable(2, h);
            }
        }

        cl->Dispatch(bucketize(equirectangularToCubemapCb.CubemapSize, 16), bucketize(equirectangularToCubemapCb.CubemapSize, 16), 6);

        mipSlice += numMips;
    }

    if (stagingResource != cubemapResource) {
        cl->CopyResource(stagingTexture.Resource, cubemap.Resource);
        cl->TransitionBarrier(cubemap.Resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }

    dxContext.RetireObject(stagingTexture.Resource);

    return cubemap;
}

DxTexture TexturePreprocessor::CubemapToIrradiance(DxCommandList *cl, DxTexture &environment, uint32 resolution, uint32 sourceSlice, float uvzScale) {
    assert(environment.Resource);

    CD3DX12_RESOURCE_DESC irradianceDesc(environment.Resource->GetDesc());

    if (IsSRGBFormat(irradianceDesc.Format)) {
        printf("Warning: Irradiance of SRGB-Format!\n");
    }

    irradianceDesc.Width = irradianceDesc.Height = resolution;
    irradianceDesc.DepthOrArraySize = 6;
    irradianceDesc.MipLevels = 1;

    DxTexture irradiance = DxTexture::Create(irradianceDesc, 0, 0);
}


