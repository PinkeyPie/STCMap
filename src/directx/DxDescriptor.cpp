
#include "DxDescriptor.h"
#include "DxTexture.h"
#include "DxBuffer.h"
#include "DxContext.h"

DxCpuDescriptorHandle &DxCpuDescriptorHandle::Create2DTextureSRV(const DxTexture *texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->Resource()->GetDesc().Format : overrideFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = mipRange.First;
    srvDesc.Texture2D.MipLevels = mipRange.Count;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateCubemapSRV(const DxTexture *texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->Resource()->GetDesc().Format : overrideFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = mipRange.First;
    srvDesc.TextureCube.MipLevels = mipRange.Count;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateCubemapArraySRV(const DxTexture *texture, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->Resource()->GetDesc().Format : overrideFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    srvDesc.TextureCubeArray.MostDetailedMip = mipRange.First;
    srvDesc.TextureCubeArray.MipLevels = mipRange.Count;
    srvDesc.TextureCubeArray.NumCubes = numCubes;
    srvDesc.TextureCubeArray.First2DArrayFace = firstCube * 6;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateVolumeTextureSRV(const DxTexture *texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->Resource()->GetDesc().Format : overrideFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MostDetailedMip = mipRange.First;
    srvDesc.Texture3D.MipLevels = mipRange.Count;
    srvDesc.Texture3D.ResourceMinLODClamp = 0;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateDepthTextureSRV(const DxTexture *texture) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DxTexture::GetDepthReadFormat(texture->Format());
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateDepthTextureArraySRV(const DxTexture *texture) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DxTexture::GetDepthReadFormat(texture->Format());
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = texture->Resource()->GetDesc().DepthOrArraySize;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateStencilTextureSRV(const DxTexture *texture) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DxTexture::GetStencilReadFormat(texture->Format());
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = 1;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource(), &srvDesc, CpuHandle);

    return *this;
}


DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateNullTextureSRV() {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 0;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(nullptr, &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateBufferSRV(const DxBuffer *buffer, BufferRange bufferRange) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = bufferRange.FirstElement;
    srvDesc.Buffer.NumElements = (bufferRange.NumElements != -1) ? bufferRange.NumElements : (buffer->ElementCount - bufferRange.FirstElement);
    srvDesc.Buffer.StructureByteStride = buffer->ElementSize;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(buffer->Resource.Get(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateRawBufferSRV(const DxBuffer *buffer, BufferRange bufferRange) {
    uint32 firstElementByteOffset = bufferRange.FirstElement * buffer->ElementSize;
    assert(firstElementByteOffset % 16 == 0);

    uint32 count = (bufferRange.NumElements != -1) ? bufferRange.NumElements : (buffer->ElementCount - bufferRange.FirstElement);
    uint32 totalSize = count * buffer->ElementSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = firstElementByteOffset / 4;
    srvDesc.Buffer.NumElements = totalSize / 4;
    srvDesc.Buffer.StructureByteStride = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(buffer->Resource.Get(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateNullBufferSRV() {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = 1;
    srvDesc.Buffer.StructureByteStride = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(0, &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::Create2DTextureUAV(const DxTexture *texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = overrideFormat;
    uavDesc.Texture2D.MipSlice = mipSlice;
    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(texture->Resource(), nullptr, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::Create2DTextureArrayUAV(const DxTexture *texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Format = overrideFormat;
    uavDesc.Texture2DArray.FirstArraySlice = 0;
    uavDesc.Texture2DArray.ArraySize = texture->Resource()->GetDesc().DepthOrArraySize;
    uavDesc.Texture2DArray.MipSlice = mipSlice;
    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(texture->Resource(), nullptr, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateCubemapUAV(const DxTexture *texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Format = overrideFormat;
    uavDesc.Texture2DArray.FirstArraySlice = 0;
    uavDesc.Texture2DArray.ArraySize = texture->Resource()->GetDesc().DepthOrArraySize;
    uavDesc.Texture2DArray.MipSlice = mipSlice;
    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(texture->Resource(), nullptr, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateVolumeTextureUAV(const DxTexture *texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    uavDesc.Format = overrideFormat;
    uavDesc.Texture3D.MipSlice = mipSlice;
    uavDesc.Texture3D.FirstWSlice = 0;
    uavDesc.Texture3D.WSize = texture->Depth();
    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(texture->Resource(), nullptr, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateNullTextureUAV() {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(0, 0, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateBufferUAV(const DxBuffer *buffer, BufferRange bufferRange) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.FirstElement = bufferRange.FirstElement;
    uavDesc.Buffer.NumElements = (bufferRange.NumElements != -1) ? bufferRange.NumElements : (buffer->ElementCount - bufferRange.FirstElement);
    uavDesc.Buffer.StructureByteStride = buffer->ElementSize;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(buffer->Resource.Get(), nullptr, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateBufferUintUAV(const DxBuffer *buffer, BufferRange bufferRange) {
    uint32 firstElementByteOffset = bufferRange.FirstElement * buffer->ElementSize;
    assert(firstElementByteOffset % 16 == 0);

    uint32 count = (bufferRange.NumElements != -1) ? bufferRange.NumElements : (buffer->ElementCount - bufferRange.FirstElement);
    uint32 totalSize = count* buffer->ElementSize;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_UINT;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.FirstElement = firstElementByteOffset / 4;
    uavDesc.Buffer.NumElements = totalSize / 4;
    uavDesc.Buffer.StructureByteStride = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(buffer->Resource.Get(), 0, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateNullBufferUAV() {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;

    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(0, 0, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateRaytracingAccelerationStructureSRV(const DxBuffer *tlas) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = tlas->GpuVirtualAddress;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(nullptr, &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle DxCpuDescriptorHandle::operator+(uint32 i) {
    return { CD3DX12_CPU_DESCRIPTOR_HANDLE(CpuHandle, i, DxContext::Instance().DescriptorHandleIncrementSize()) };
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::operator+=(uint32 i) {
    CpuHandle.Offset(i, DxContext::Instance().DescriptorHandleIncrementSize());
    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::operator++() {
    CpuHandle.Offset(DxContext::Instance().DescriptorHandleIncrementSize());
    return *this;
}

DxCpuDescriptorHandle DxCpuDescriptorHandle::operator++(int) {
    DxCpuDescriptorHandle result = *this;
    CpuHandle.Offset(DxContext::Instance().DescriptorHandleIncrementSize());
    return result;
}

DxGpuDescriptorHandle DxGpuDescriptorHandle::operator+(uint32 i) {
    return { CD3DX12_GPU_DESCRIPTOR_HANDLE(GpuHandle, i, DxContext::Instance().DescriptorHandleIncrementSize()) };
}

DxGpuDescriptorHandle &DxGpuDescriptorHandle::operator+=(uint32 i) {
    GpuHandle.Offset(i, DxContext::Instance().DescriptorHandleIncrementSize());
    return *this;
}

DxGpuDescriptorHandle &DxGpuDescriptorHandle::operator++() {
    GpuHandle.Offset(DxContext::Instance().DescriptorHandleIncrementSize());
    return *this;
}

DxGpuDescriptorHandle DxGpuDescriptorHandle::operator++(int) {
    DxGpuDescriptorHandle result = *this;
    GpuHandle.Offset(DxContext::Instance().DescriptorHandleIncrementSize());
    return result;
}

DxRtvDescriptorHandle &DxRtvDescriptorHandle::Create2DTextureRTV(const DxTexture *texture, uint32 arraySlice, uint32 mipSlice) {
    assert(texture->SupportsRTV());
    DxContext& context = DxContext::Instance();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Format = texture->Format();

    if (texture->Depth() == 1) {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = mipSlice;
        rtvDesc.Texture2D.PlaneSlice = 0;

        context.GetDevice()->CreateRenderTargetView(texture->Resource(), &rtvDesc, CpuHandle);
    }
    else {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.MipSlice = mipSlice;
        rtvDesc.Texture2DArray.PlaneSlice = 0;

        context.GetDevice()->CreateRenderTargetView(texture->Resource(), &rtvDesc, CpuHandle);
    }

    return *this;
}

DxDsvDescriptorHandle &DxDsvDescriptorHandle::Create2DTextureDSV(const DxTexture *texture, uint32 arraySlice, uint32 mipSlice) {
    assert(texture->SupportsDSV());
    assert(DxTexture::IsDepthFormat(texture->Format()));
    DxContext& context = DxContext::Instance();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = texture->Format();

    if (texture->Depth() == 1) {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = mipSlice;

        context.GetDevice()->CreateDepthStencilView(texture->Resource(), &dsvDesc, CpuHandle);
    }
    else {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
        dsvDesc.Texture2DArray.ArraySize = 1;
        dsvDesc.Texture2DArray.MipSlice = mipSlice;

        context.GetDevice()->CreateDepthStencilView(texture->Resource(), &dsvDesc, CpuHandle);
    }

    return *this;
}

