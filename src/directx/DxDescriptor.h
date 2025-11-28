#pragma once

#include "../pch.h"
#include "dx.h"

class DxTexture;

struct TextureMipRange {
    uint32 First = 0;
    uint32 Count = (uint32)-1; // Use all mips
};

class DxBuffer;

struct BufferRange {
    uint32 FirstElement = 0;
    uint32 NumElements = (uint32)-1;
};

struct DxCpuDescriptorHandle {
    DxCpuDescriptorHandle() = default;
    DxCpuDescriptorHandle(CD3DX12_CPU_DESCRIPTOR_HANDLE handle) : CpuHandle(handle) {}
    DxCpuDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) : CpuHandle(handle) {}
    DxCpuDescriptorHandle(CD3DX12_DEFAULT) : CpuHandle(D3D12_DEFAULT) {}

    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle;

    DxCpuDescriptorHandle& Create2DTextureSRV(const DxTexture *texture, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateCubemapSRV(const DxTexture *texture, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateCubemapArraySRV(const DxTexture *texture, TextureMipRange mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateVolumeTextureSRV(const DxTexture* texture, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateDepthTextureSRV(const DxTexture *texture);
    DxCpuDescriptorHandle& CreateDepthTextureArraySRV(const DxTexture* texture);
    DxCpuDescriptorHandle& CreateStencilTextureSRV(const DxTexture* texture);
    DxCpuDescriptorHandle& CreateNullTextureSRV();
    DxCpuDescriptorHandle& CreateBufferSRV(const DxBuffer *buffer, BufferRange bufferRange = {});
    DxCpuDescriptorHandle& CreateRawBufferSRV(const DxBuffer *buffer, BufferRange bufferRange = {});
    DxCpuDescriptorHandle& CreateNullBufferSRV();
    DxCpuDescriptorHandle& Create2DTextureUAV(const DxTexture *texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& Create2DTextureArrayUAV(const DxTexture *texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateCubemapUAV(const DxTexture *texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateVolumeTextureUAV(const DxTexture* texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
    DxCpuDescriptorHandle& CreateNullTextureUAV();
    DxCpuDescriptorHandle& CreateBufferUAV(const DxBuffer *buffer, BufferRange bufferRange = {});
    DxCpuDescriptorHandle& CreateBufferUintUAV(const DxBuffer *buffer, BufferRange bufferRange = {});
    DxCpuDescriptorHandle& CreateNullBufferUAV();
    DxCpuDescriptorHandle& CreateRaytracingAccelerationStructureSRV(const DxBuffer *tlas);

    operator CD3DX12_CPU_DESCRIPTOR_HANDLE() const { return CpuHandle; }
    operator bool() const { return CpuHandle.ptr != 0; }

    DxCpuDescriptorHandle operator+(uint32 i);
    DxCpuDescriptorHandle& operator+=(uint32 i);
    DxCpuDescriptorHandle& operator++();
    DxCpuDescriptorHandle operator++(int);
};

struct DxGpuDescriptorHandle {
    DxGpuDescriptorHandle() = default;
    DxGpuDescriptorHandle(CD3DX12_GPU_DESCRIPTOR_HANDLE handle) : GpuHandle(handle) {}
    DxGpuDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) : GpuHandle(handle) {}
    DxGpuDescriptorHandle(CD3DX12_DEFAULT) : GpuHandle(D3D12_DEFAULT) {}

    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle;

    operator CD3DX12_GPU_DESCRIPTOR_HANDLE() const { return GpuHandle; }
    operator bool() const { return GpuHandle.ptr != 0; }

    DxGpuDescriptorHandle operator+(uint32 i);
    DxGpuDescriptorHandle& operator+=(uint32 i);
    DxGpuDescriptorHandle& operator++();
    DxGpuDescriptorHandle operator++(int);

};

struct DxDoubleDescriptorHandle : DxCpuDescriptorHandle, DxGpuDescriptorHandle {};

struct DxRtvDescriptorHandle {
    DxRtvDescriptorHandle() = default;
    DxRtvDescriptorHandle(CD3DX12_CPU_DESCRIPTOR_HANDLE handle) : CpuHandle(handle) {}
    DxRtvDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) : CpuHandle(handle) {}
    DxRtvDescriptorHandle(CD3DX12_DEFAULT) : CpuHandle(D3D12_DEFAULT) {}

    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle;

    operator CD3DX12_CPU_DESCRIPTOR_HANDLE() const { return CpuHandle; }
    operator bool() const { return CpuHandle.ptr != 0; }

    DxRtvDescriptorHandle& Create2DTextureRTV(const DxTexture* texture, uint32 arraySlice = 0, uint32 mipSlice = 0);
};

struct DxDsvDescriptorHandle {
    DxDsvDescriptorHandle() = default;
    DxDsvDescriptorHandle(CD3DX12_CPU_DESCRIPTOR_HANDLE handle) : CpuHandle(handle) {}
    DxDsvDescriptorHandle(CD3DX12_DEFAULT) : CpuHandle(D3D12_DEFAULT) {}

    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle;

    operator CD3DX12_CPU_DESCRIPTOR_HANDLE() const { return CpuHandle; }
    operator bool() const { return CpuHandle.ptr != 0; }

    DxDsvDescriptorHandle& Create2DTextureDSV(const DxTexture* texture, uint32 arraySlice = 0, uint32 mipSlice = 0);
};