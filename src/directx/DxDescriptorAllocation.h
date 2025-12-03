#pragma once

#include <stack>

#include "dx.h"
#include "DxDescriptor.h"
#include "../core/threading.h"

void CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC& desc, Com<ID3D12DescriptorHeap>& heap);
uint32 GetIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type);

template<class DescriptorT>
class DxDescriptorHeap {
public:
    void Initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible);
    DxDescriptorHeap() = default;
    DescriptorT GetFreeHandle();
    void FreeHandle(DescriptorT handle);
    ID3D12DescriptorHeap* DescriptorHeap() const { return _descriptorHeap.Get(); }

    DxGpuDescriptorHandle GetMatchingGpuHandle(DxCpuDescriptorHandle handle);
    DxCpuDescriptorHandle GetMatchingCpuHandle(DxGpuDescriptorHandle handle);

protected:
    D3D12_DESCRIPTOR_HEAP_TYPE Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    Com<ID3D12DescriptorHeap> _descriptorHeap = nullptr;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _cpuBase = D3D12_DEFAULT;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _gpuBase = D3D12_DEFAULT;
    uint32 _descriptorHandleIncrementSize = 0;
    std::vector<uint16> _freeDescriptors = {};
    uint32 _allFreeIncludingAndAfter = 0;

    DxCpuDescriptorHandle GetHandle(uint32 index) { return {CD3DX12_CPU_DESCRIPTOR_HANDLE(_cpuBase, index, _descriptorHandleIncrementSize)}; }
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

class DxFrameDescriptorAllocator {
public:
    DxFrameDescriptorAllocator() = default;
    void NewFrame(uint32 bufferedFrameId);
    DxDescriptorRange AllocateContiguousDescriptorRange(uint32 count);

protected:
    std::mutex _mutex = {};
    std::stack<std::unique_ptr<DxDescriptorPage>> _usedPages[NUM_BUFFERED_FRAMES] = {};
    std::stack<std::unique_ptr<DxDescriptorPage>> _freePages = {};
    uint32 _currentFrame = NUM_BUFFERED_FRAMES - 1;
};

struct DxPushableResourceDescriptorHeap {
    void Inititalize(uint32 maxSize, bool shaderVisible = true);
    DxCpuDescriptorHandle Push();

    Com<ID3D12DescriptorHeap> DescriptorHeap;
    DxCpuDescriptorHandle CurrentCPU;
    DxGpuDescriptorHandle CurrentGpu;
};

template<class DescriptorT>
void DxDescriptorHeap<DescriptorT>::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;
    if (shaderVisible) {
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }

    CreateDescriptorHeap(desc, _descriptorHeap);
    _allFreeIncludingAndAfter = 0;
    _descriptorHandleIncrementSize = GetIncrementSize(type);
    _cpuBase = _descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible) {
        _gpuBase = _descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    }
    Type = type;
}

template<class DescriptorT>
DescriptorT DxDescriptorHeap<DescriptorT>::GetFreeHandle() {
    uint32 index;
    if (!_freeDescriptors.empty()) {
        index = _freeDescriptors.back();
        _freeDescriptors.pop_back();
    }
    else {
        index = AtomicAdd(_allFreeIncludingAndAfter, 1);
    }
    return { CD3DX12_CPU_DESCRIPTOR_HANDLE(_cpuBase, index, _descriptorHandleIncrementSize)};
}

template<class DescriptorT>
void DxDescriptorHeap<DescriptorT>::FreeHandle(DescriptorT handle) {
    uint32 index = (uint32)((handle.CpuHandle.ptr - _cpuBase.ptr) / _descriptorHandleIncrementSize);
    _freeDescriptors.push_back(index);
}

template<class DescriptorT>
DxCpuDescriptorHandle DxDescriptorHeap<DescriptorT>::GetMatchingCpuHandle(DxGpuDescriptorHandle handle) {
    uint32 offset = (uint32)(handle.GpuHandle.ptr - _gpuBase.ptr);
    return { CD3DX12_CPU_DESCRIPTOR_HANDLE(_cpuBase, offset)};
}

template<class DescriptorT>
DxGpuDescriptorHandle DxDescriptorHeap<DescriptorT>::GetMatchingGpuHandle(DxCpuDescriptorHandle handle) {
    uint32 offset = (uint32)(handle.CpuHandle.ptr - _cpuBase.ptr);
    return {CD3DX12_GPU_DESCRIPTOR_HANDLE(_gpuBase, offset)};
}