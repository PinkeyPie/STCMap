#pragma once

#include "../pch.h"
#include "dx.h"
#include <deque>

#include "DxDescriptor.h"

class DxCommandList;
class DxRootSignature;

class DxDynamicDescriptorHeap {
public:
    void Initialize(uint32 numDescriptorsPerHeap = 1024);

    void StageDescriptors(uint32 rootParameterIndex, uint32 offset, uint32 numDescriptors, DxCpuDescriptorHandle srcDescriptor);

    void CommitStagedDescriptorsForDraw(DxCommandList* commandList);
    void CommitStagedDescriptorsForDispatch(DxCommandList* commandList);

    void SetCurrentDescriptorHeap(DxCommandList* commandList);

    void ParseRootSignature(const DxRootSignature& rootSignature);

    void Reset();

private:
    Com<ID3D12DescriptorHeap> RequestDescriptorHeap();
    Com<ID3D12DescriptorHeap> CreateDescriptorHeap();

    uint32 ComputeStaleDescriptorCount() const;

    void CommitStagedDescriptors(DxCommandList* commandList, bool graphics);

    static constexpr uint32 maxDescriptorTables = 32;

    struct DescriptorTableCache {
        DescriptorTableCache() : NumDescriptors(0), BaseDescriptor(nullptr) {}

        // Reset the table cache
        void Reset() {
            NumDescriptors = 0;
            BaseDescriptor = nullptr;
        }

        uint32 NumDescriptors;
        D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor;
    };

    uint32 _numDescriptorsPerHeap;
    uint32 _descriptorHandleIncrementSize;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> _descriptorHandleCache;
    DescriptorTableCache _descriptorTableCache[maxDescriptorTables];

    // Each bit in the bit mask represents the index in the root signature that contains a descriptor table
    uint32 _descriptorTableBitMask;
    uint32 _staleDescriptorTableBitMask;

    std::vector<Com<ID3D12DescriptorHeap>> _descriptorHeapPool;
    std::vector<Com<ID3D12DescriptorHeap>> _freeDescriptorHeaps;

    Com<ID3D12DescriptorHeap> _currentDescriptorHeap;
    DxCpuDescriptorHandle _currentCpuDescriptorHandle;
    DxGpuDescriptorHandle _currentGpuDescriptorHandle;

    uint32 _numFreeHandles;
};
