
#include "DxDescriptor.h"

#include "DxRenderPrimitives.h"
#include "DxContext.h"

DxCpuDescriptorHandle &DxCpuDescriptorHandle::Create2DTextureSRV(DxTexture *texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->Resource->GetDesc().Format : overrideFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = mipRange.First;
    srvDesc.Texture2D.MipLevels = mipRange.Count;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource.Get(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateCubemapSRV(DxTexture *texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->Resource->GetDesc().Format : overrideFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = mipRange.First;
    srvDesc.TextureCube.MipLevels = mipRange.Count;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource.Get(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateCubemapArraySRV(DxTexture *texture, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->Resource->GetDesc().Format : overrideFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    srvDesc.TextureCubeArray.MostDetailedMip = mipRange.First;
    srvDesc.TextureCubeArray.MipLevels = mipRange.Count;
    srvDesc.TextureCubeArray.NumCubes = numCubes;
    srvDesc.TextureCubeArray.First2DArrayFace = firstCube * 6;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource.Get(), &srvDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateDepthTextureSRV(DxTexture *texture) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = GetReadFormatFromTypeless(texture->Resource->GetDesc().Format);
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(texture->Resource.Get(), &srvDesc, CpuHandle);

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

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateBufferSRV(DxBuffer *buffer, BufferRange bufferRange) {
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

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateRawBufferSRV(DxBuffer *buffer, BufferRange bufferRange) {
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

DxCpuDescriptorHandle &DxCpuDescriptorHandle::Create2DTextureUAV(DxTexture *texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = overrideFormat;
    uavDesc.Texture2D.MipSlice = mipSlice;
    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(texture->Resource.Get(), nullptr, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::Create2DTextureArrayUAV(DxTexture *texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Format = overrideFormat;
    uavDesc.Texture2DArray.FirstArraySlice = 0;
    uavDesc.Texture2DArray.ArraySize = texture->Resource->GetDesc().DepthOrArraySize;
    uavDesc.Texture2DArray.MipSlice = mipSlice;
    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(texture->Resource.Get(), nullptr, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateCubemapUAV(DxTexture *texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Format = overrideFormat;
    uavDesc.Texture2DArray.FirstArraySlice = 0;
    uavDesc.Texture2DArray.ArraySize = texture->Resource->GetDesc().DepthOrArraySize;
    uavDesc.Texture2DArray.MipSlice = mipSlice;
    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(texture->Resource.Get(), nullptr, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateNullTextureUAV() {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    DxContext::Instance().GetDevice()->CreateUnorderedAccessView(0, 0, &uavDesc, CpuHandle);

    return *this;
}

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateBufferUAV(DxBuffer *buffer, BufferRange bufferRange) {
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

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateBufferUintUAV(DxBuffer *buffer, BufferRange bufferRange) {
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

DxCpuDescriptorHandle &DxCpuDescriptorHandle::CreateRaytracingAccelerationStructureSRV(DxBuffer &tlas) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = tlas.GpuVirtualAddress;

    DxContext::Instance().GetDevice()->CreateShaderResourceView(nullptr, &srvDesc, CpuHandle);

    return *this;
}
