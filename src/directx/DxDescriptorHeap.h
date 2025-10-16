#pragma once

#include <mutex>

#include "../pch.h"
#include "../core/threading.h"
#include "dx.h"
#include "DxBuffer.h"

class DxTexture;

struct TextureMipRange {
	uint32 First = 0;
	uint32 Count = (uint32)-1; // Use all mips
};

struct DxDescriptorHandle {
	CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle;
};

class DxDescriptorHeap {
public:
	Com<ID3D12DescriptorHeap> DescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE Type;
	DxDescriptorHeap() = default;
	DxDescriptorHeap(uint32 maxNumDescriptors, uint32 descriptorHandleIncrementSize) :
	_maxNumDescriptors(maxNumDescriptors), _descriptorHandleIncrementSize(descriptorHandleIncrementSize) {}

	[[nodiscard]] DxDescriptorHandle GetHandle(const uint32 index) const {
		assert(index < _maxNumDescriptors);

		const CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(_base.GpuHandle, index, _descriptorHandleIncrementSize);
		const CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(_base.CpuHandle, index, _descriptorHandleIncrementSize);

		return {.CpuHandle = cpuHandle, .GpuHandle = gpuHandle };
	}
	void SetBase(DxDescriptorHandle handle);

protected:
	void Initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible);

	uint32 _maxNumDescriptors = 0;
	uint32 _descriptorHandleIncrementSize = 0;
	DxDescriptorHandle _base = {};
};

class DxDescriptorRange : public DxDescriptorHeap {
public:
	DxDescriptorRange() = default;
	DxDescriptorRange(uint32 maxNumDescriptors, uint32 descriptorHandleIncrementSize) : DxDescriptorHeap(maxNumDescriptors, descriptorHandleIncrementSize) {}

	DxDescriptorHandle PushEmptyHandle();

	DxDescriptorHandle Create2DTextureSRV(DxTexture& texture, DxDescriptorHandle handle, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle Create2DTextureSRV(DxTexture& texture, uint32 index, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle Push2DTextureSRV(DxTexture& texture, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	DxDescriptorHandle CreateCubemapSRV(DxTexture& texture, DxDescriptorHandle handle, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle CreateCubemapSRV(DxTexture& texture, uint32 index, TextureMipRange mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle PushCubemapSRV(DxTexture& texture, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	DxDescriptorHandle CreateCubemapArraySRV(DxTexture& texture, DxDescriptorHandle handle, TextureMipRange mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle CreateCubemapArraySRV(DxTexture& texture, uint32 index, TextureMipRange mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle PushCubemapArraySRV(DxTexture& texture, TextureMipRange mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	DxDescriptorHandle CreateDepthTextureSRV(DxTexture& texture, DxDescriptorHandle handle);
	DxDescriptorHandle CreateDepthTextureSRV(DxTexture& texture, uint32 index);
	DxDescriptorHandle PushDepthTextureSRV(DxTexture& texture);

	DxDescriptorHandle CreateNullTextureSRV(DxDescriptorHandle handle);
	DxDescriptorHandle CreateNullTextureSRV(uint32 index);
	DxDescriptorHandle PushNullTextureSRV();

	DxDescriptorHandle CreateBufferSRV(DxBuffer& buffer, DxDescriptorHandle handle, BufferRange bufferRange = {});
	DxDescriptorHandle CreateBufferSRV(DxBuffer& buffer, uint32 index, BufferRange bufferRange = {});
	DxDescriptorHandle PushBufferSRV(DxBuffer& buffer, BufferRange bufferRange = {});

	DxDescriptorHandle CreateRawBufferSRV(DxBuffer& buffer, DxDescriptorHandle handle, BufferRange bufferRange = {});
	DxDescriptorHandle CreateRawBufferSRV(DxBuffer& buffer, uint32 index, BufferRange bufferRange = {});
	DxDescriptorHandle PushRawBufferSRV(DxBuffer& buffer, BufferRange bufferRange = {});

	DxDescriptorHandle Create2DTextureUAV(DxTexture& texture, DxDescriptorHandle handle, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle Create2DTextureUAV(DxTexture& texture, uint32 index, uint32 mipSlice, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle Push2DTextureUAV(DxTexture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	DxDescriptorHandle Create2DTextureArrayUAV(DxTexture& texture, DxDescriptorHandle handle, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle Create2DTextureArrayUAV(DxTexture& texture, uint32 index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	DxDescriptorHandle Push2DTextureArrayUAV(DxTexture& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	DxDescriptorHandle CreateNullTextureUAV(DxDescriptorHandle handle);
	DxDescriptorHandle CreateNullTextureUAV(uint32 index);
	DxDescriptorHandle PushNullTextureUAV();

	DxDescriptorHandle CreateBufferUAV(DxBuffer& buffer, DxDescriptorHandle handle, BufferRange bufferRange = {});
	DxDescriptorHandle CreateBufferUAV(DxBuffer& buffer, uint32 index, BufferRange bufferRange = {});
	DxDescriptorHandle PushBufferUAV(DxBuffer& buffer, BufferRange bufferRange = {});

	DxDescriptorHandle CreateRaytracingAccelerationStructureSRV(DxBuffer& tlas, DxDescriptorHandle handle);
	DxDescriptorHandle CreateRaytracingAccelerationStructureSRV(DxBuffer& tlas, uint32 index);
	DxDescriptorHandle PushRaytracingAccelerationStructureSRV(DxBuffer& tlas);

protected:
	uint32 _pushIndex = 0;
};

class DxCbvSrvUavDescriptorHeap : public DxDescriptorRange {
public:
	static DxCbvSrvUavDescriptorHeap Create(uint32 numDescriptors, bool shaderVisible = true);
};

struct DxDescriptorPage {
	Com<ID3D12DescriptorHeap> DescriptorHeap;
	DxDescriptorHandle Base;
	uint32 UsedDescriptors;
	uint32 MaxNumDescriptors;
	uint32 DescriptorHandleIncrementSize;

	DxDescriptorPage* Next;
};

class DxFrameDescriptorAllocator {
public:
	DxFrameDescriptorAllocator() = default;

	void NewFrame(uint32 bufferedFrameId);
	void SetDevice(DxDevice device) {
		_device = device;
	}

	DxDescriptorRange AllocateContiguousDescriptorRange(uint32 count);

protected:
	DxDevice _device = nullptr;

	std::mutex _mutex = {};
	DxDescriptorPage* _usedPages[NUM_BUFFERED_FRAMES] = {};
	DxDescriptorPage* _freePages = nullptr;
	uint32 _currentFrame = NUM_BUFFERED_FRAMES - 1;
};

class DxRtvDescriptorHeap : public DxDescriptorHeap {
public:
	DxDescriptorHandle PushRenderTargetView(DxTexture* texture);
	DxDescriptorHandle CreateRenderTargetView(DxTexture* texture, DxDescriptorHandle index);

	static DxRtvDescriptorHeap CreateRTVDescriptorAllocator(uint32 numDescriptors);

private:
	volatile uint32 _pushIndex = 0;
};

class DxDsvDescriptorHeap : public DxDescriptorHeap {
public:
	DxDescriptorHandle PushDepthStencilView(DxTexture* texture);
	DxDescriptorHandle CreateDepthStencilView(DxTexture* texture, DxDescriptorHandle index);

	static DxDsvDescriptorHeap CreateDSVDescriptorAllocator(uint32 numDescriptors);

private:
	volatile uint32 _pushIndex = 0;
};