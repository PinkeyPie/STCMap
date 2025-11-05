#pragma once

#include "../pch.h"
#include "dx.h"
#include "DxBuffer.h"

class DxTexture;

struct TextureMipRange {
    uint32 First = 0;
    uint32 Count = (uint32)-1; // Use all mips
};

struct DxCpuDescriptorHandle {
    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle;

    DxCpuDescriptorHandle& Create2DTextureSRV(DxTexture* texture, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateCubemapSRV(DxTexture* texture, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateCubemapArraySRV(DxTexture* texture, TextureMipRange mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateDepthTextureSRV(DxTexture* texture);
    DxCpuDescriptorHandle& CreateNullTextureSRV();
    DxCpuDescriptorHandle& CreateBufferSRV(DxBuffer* buffer, BufferRange bufferRange = {});
    DxCpuDescriptorHandle& CreateRawBufferSRV(DxBuffer* buffer, BufferRange bufferRange = {});
    DxCpuDescriptorHandle& Create2DTextureUAV(DxTexture* texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& Create2DTextureArrayUAV(DxTexture* texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateCubemapUAV(DxTexture* texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateNullTextureUAV();
    DxCpuDescriptorHandle& CreateBufferUAV(DxBuffer* buffer, BufferRange bufferRange = {});
    DxCpuDescriptorHandle& CreateBufferUintUAV(DxBuffer* buffer, BufferRange bufferRange = {});
    DxCpuDescriptorHandle& CreateRaytracingAccelerationStructureSRV(DxBuffer& tlas);

    operator CD3DX12_CPU_DESCRIPTOR_HANDLE() const { return CpuHandle; }
};

struct DxGpuDescriptorHandle {
    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle;

    operator CD3DX12_GPU_DESCRIPTOR_HANDLE() const { return GpuHandle; }
};

struct DxDoubleDescriptorHandle : DxCpuDescriptorHandle, DxGpuDescriptorHandle {
};