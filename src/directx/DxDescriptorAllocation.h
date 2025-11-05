#pragma once

#include <mutex>
#include <stack>

#include "dx.h"
#include "DxDescriptor.h"

class DxDescriptorHeap {
public:
    void Initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible);
    DxDescriptorHeap() = default;
    DxCpuDescriptorHandle GetFreeHandle();
    void FreeHandle(DxCpuDescriptorHandle handle);

    DxGpuDescriptorHandle GetMatchingGpuHandle(DxCpuDescriptorHandle handle);
    DxCpuDescriptorHandle GetMatchingCpuHandle(DxGpuDescriptorHandle handle);

    D3D12_DESCRIPTOR_HEAP_TYPE Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    ID3D12DescriptorHeap* DescriptorHeap() const { return _descriptorHeap.Get(); }
    uint32 GetIncrementSize() const { return _descriptorHandleIncrementSize; }

protected:
    CD3DX12_CPU_DESCRIPTOR_HANDLE _cpuBase = D3D12_DEFAULT;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _gpuBase = D3D12_DEFAULT;
    uint32 _descriptorHandleIncrementSize = 0;
    std::vector<uint16> _freeDescriptors = {};
    uint16 _allFreeIncludingAndAfter = 0;
    Com<ID3D12DescriptorHeap> _descriptorHeap = nullptr;

    DxCpuDescriptorHandle GetHandle(uint32 index) { return {CD3DX12_CPU_DESCRIPTOR_HANDLE(_cpuBase, index, _descriptorHandleIncrementSize)}; }
};

class DxRtvDescriptorHeap : public DxDescriptorHeap {
public:
    void Initialize(uint32 numDescriptors, bool shaderVisible = true);

    DxCpuDescriptorHandle PushRenderTargetView(DxTexture* texture);
    DxCpuDescriptorHandle CreateRenderTargetView(DxTexture* texture, DxCpuDescriptorHandle index);

private:
    volatile uint32 _pushIndex = 0;
};

class DxDsvDescriptorHeap : public DxDescriptorHeap {
public:
    void Initialize(uint32 numDescriptors, bool shaderVisible = true);

    DxCpuDescriptorHandle PushDepthStencilView(DxTexture* texture);
    DxCpuDescriptorHandle CreateDepthStencilView(DxTexture* texture, DxCpuDescriptorHandle index);

private:
    volatile uint32 _pushIndex = 0;
};

class DxDescriptorPage {
public:
    DxDescriptorPage(uint32 maxNumDescriptors);

    void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    DxDescriptorRange GetRange(uint32 count);
    bool HaveEnoughSpace(uint32 count) const;
    void Reset();

private:
    Com<ID3D12DescriptorHeap> _descriptorHeap = nullptr;
    DxDoubleDescriptorHandle _base;
    uint32 _usedDescriptors = 0;
    uint32 _maxNumDescriptors = 0;
    uint32 _descriptorHandleIncrementSize = 0;
};

class DxDescriptorRange {
public:
    DxDescriptorRange(uint32 count, uint32 descriptorHandleIncrementSize) : _maxNumDescriptors(count),
    _descriptorHandleIncrementSize(descriptorHandleIncrementSize) {};

    DxDoubleDescriptorHandle PushHandle() {
        DxDoubleDescriptorHandle result = {
            CD3DX12_CPU_DESCRIPTOR_HANDLE(_base.CpuHandle, _pushIndex, _descriptorHandleIncrementSize),
            CD3DX12_GPU_DESCRIPTOR_HANDLE(_base.GpuHandle, _pushIndex, _descriptorHandleIncrementSize)
        };
        _pushIndex++;
        return result;
    }
    void SetBase(CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
        _base.CpuHandle = cpuHandle;
        _base.GpuHandle = gpuHandle;
    }

    uint32 GetIncrementSize() const { return _descriptorHandleIncrementSize; }

    Com<ID3D12DescriptorHeap> DescriptorHeap;
    D3D12_DESCRIPTOR_HEAP_TYPE Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

protected:
    DxDoubleDescriptorHandle _base = {};
    uint32 _pushIndex = 0;
    uint32 _maxNumDescriptors;
    uint32 _descriptorHandleIncrementSize = 0;

    friend class DxFrameDescriptorAllocator;
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
    std::stack<DxDescriptorPage*> _usedPages[NUM_BUFFERED_FRAMES] = {};
    std::stack<DxDescriptorPage*> _freePages = {};
    uint32 _currentFrame = NUM_BUFFERED_FRAMES - 1;
};