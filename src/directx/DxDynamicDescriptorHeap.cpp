//
// Created by Chingis on 05.11.2025.
//

#include "DxDynamicDescriptorHeap.h"
#include "DxDynamicDescriptorHeap.h"
#include "DxCommandList.h"
#include "DxRenderPrimitives.h"
#include "DxContext.h"

void DxDynamicDescriptorHeap::Initialize(uint32 numDescriptorsPerHeap) {
    _numDescriptorsPerHeap = numDescriptorsPerHeap;
    _descriptorTableBitMask = 0;
    _staleDescriptorTableBitMask = 0;
    _currentCpuDescriptorHandle = D3D12_DEFAULT;
    _currentGpuDescriptorHandle = D3D12_DEFAULT;
    _numFreeHandles = 0;

    _descriptorHandleIncrementSize = DxContext::Instance().GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    _descriptorHandleCache.resize(_numDescriptorsPerHeap);
}



void DxDynamicDescriptorHeap::StageDescriptors(uint32 rootParameterIndex, uint32 offset, uint32 numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor) {
    assert(numDescriptors <= _numDescriptorsPerHeap and rootParameterIndex < maxDescriptorTables);

    DescriptorTableCache& cache = _descriptorTableCache[rootParameterIndex];

    assert((offset + numDescriptors) <= cache.NumDescriptors);

    D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (cache.BaseDescriptor + offset);
    for (uint32 i = 0; i < numDescriptors; i++) {
        dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptor, i, _descriptorHandleIncrementSize);
    }

    SetBit(_staleDescriptorTableBitMask, rootParameterIndex);
}

void DxDynamicDescriptorHeap::CommitStagedDescriptors(DxCommandList *commandList, bool graphics) {
    uint32 numDescriptorsToCommit = ComputeStaleDescriptorCount();

    if (numDescriptorsToCommit > 0) {
        if (!_currentDescriptorHeap or _numFreeHandles < numDescriptorsToCommit) {
            _currentDescriptorHeap = RequestDescriptorHeap();
            _currentCpuDescriptorHandle = _currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            _currentGpuDescriptorHandle = _currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
            _numFreeHandles = _numDescriptorsPerHeap;

            commandList->SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, _currentDescriptorHeap.Get());

            // When updating the descriptor heap on the command list, all descriptor tables
            // must be (re)recopied to the new descriptor heap (not just the stale descriptor tables).
            _staleDescriptorTableBitMask = _descriptorTableBitMask;
        }

        DWORD rootIndex;
        while (_BitScanForward(&rootIndex, _staleDescriptorTableBitMask)) {
            uint32 numSrcDescriptors = _descriptorTableCache[rootIndex].NumDescriptors;
            D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorHandles = _descriptorTableCache[rootIndex].BaseDescriptor;
            D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart = _currentCpuDescriptorHandle;

            DxContext::Instance().GetDevice()->CopyDescriptors(
                1, &destDescriptorRangeStart, &numSrcDescriptors,
                numSrcDescriptors, srcDescriptorHandles, nullptr,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );
            if (graphics) {
                commandList->SetGraphicsDescriptorTable(rootIndex, _currentGpuDescriptorHandle);
            }
            else {
                commandList->SetComputeDescriptorTable(rootIndex, _currentGpuDescriptorHandle);
            }

            _currentCpuDescriptorHandle.Offset(numSrcDescriptors, _descriptorHandleIncrementSize);
            _currentGpuDescriptorHandle.Offset(numSrcDescriptors, _descriptorHandleIncrementSize);
            _numFreeHandles -= numSrcDescriptors;

            UnsetBit(_staleDescriptorTableBitMask, rootIndex);
        }
    }
}

void DxDynamicDescriptorHeap::CommitStagedDescriptorsForDraw(DxCommandList *commandList) {
    CommitStagedDescriptors(commandList, true);
}

void DxDynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(DxCommandList *commandList) {
    CommitStagedDescriptors(commandList, false);
}

void DxDynamicDescriptorHeap::SetCurrentDescriptorHeap(DxCommandList *commandList) {
    commandList->SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, _currentDescriptorHeap.Get());
}

void DxDynamicDescriptorHeap::ParseRootSignature(const DxRootSignature &rootSignature) {
    _staleDescriptorTableBitMask = 0;

    _descriptorTableBitMask = rootSignature.TableRootParameterMask();
    uint32 numDescriptorTables = rootSignature.NumDescriptorTables();

    uint32 bitmask = _descriptorTableBitMask;
    uint32 currentOffset = 0;
    DWORD rootIndex;
    uint32 descriptorTableIndex = 0;
    while (_BitScanForward(&rootIndex, bitmask)) {
        uint32 numDescriptors = rootSignature.DescriptorTableSize()[descriptorTableIndex++];

        DescriptorTableCache& cache = _descriptorTableCache[rootIndex];
        cache.NumDescriptors = numDescriptors;
        cache.BaseDescriptor = &_descriptorHandleCache[currentOffset];

        currentOffset += numDescriptors;

        // Flip the descriptor table bit so it's not scanned again for the current index.
        UnsetBit(bitmask, rootIndex);
    }

    assert(currentOffset <= _numDescriptorsPerHeap and
        "The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void DxDynamicDescriptorHeap::Reset() {
    _freeDescriptorHeaps = _descriptorHeapPool;
    _currentDescriptorHeap.Reset();
    _currentCpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    _currentGpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    _numFreeHandles = 0;
    _descriptorTableBitMask = 0;
    _staleDescriptorTableBitMask = 0;

    // Reset the table cache
    for (int i = 0; i < maxDescriptorTables; i++) {
        _descriptorTableCache[i].Reset();
    }
}

Com<ID3D12DescriptorHeap> DxDynamicDescriptorHeap::RequestDescriptorHeap() {
    Com<ID3D12DescriptorHeap> descriptorHeap;
    if (_freeDescriptorHeaps.size() > 0) {
        descriptorHeap = _freeDescriptorHeaps.back();
        _freeDescriptorHeaps.pop_back();
    }
    else {
        descriptorHeap = CreateDescriptorHeap();
        _descriptorHeapPool.push_back(descriptorHeap);
    }

    return descriptorHeap;
}

Com<ID3D12DescriptorHeap> DxDynamicDescriptorHeap::CreateDescriptorHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.NumDescriptors = _numDescriptorsPerHeap;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    Com<ID3D12DescriptorHeap> descriptorHeap;
    ThrowIfFailed(DxContext::Instance().GetDevice()->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf())));

    return descriptorHeap;
}

uint32 DxDynamicDescriptorHeap::ComputeStaleDescriptorCount() const {
    uint32 numStaleDescriptors = 0;
    DWORD i;
    DWORD staleDescriptorsBitMask = _staleDescriptorTableBitMask;

    while (_BitScanForward(&i, staleDescriptorsBitMask)) {
        numStaleDescriptors += _descriptorTableCache[i].NumDescriptors;
        UnsetBit(staleDescriptorsBitMask, i);
    }

    return numStaleDescriptors;
}

