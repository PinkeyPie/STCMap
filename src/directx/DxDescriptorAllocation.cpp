#include "DxDescriptorAllocation.h"
#include "DxContext.h"

DxDescriptorPage::DxDescriptorPage(uint32 maxNumDescriptors) : _maxNumDescriptors(maxNumDescriptors) {}

void DxDescriptorPage::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = _maxNumDescriptors;
    desc.Type = type;
    desc.Flags = flags | D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_descriptorHeap.GetAddressOf())));

    _descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(desc.Type);
    _base.CpuHandle = _descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    _base.GpuHandle = _descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

DxDescriptorRange DxDescriptorPage::GetRange(uint32 count) {
    DxDescriptorRange result(count, _descriptorHandleIncrementSize);
    result.SetBase(CD3DX12_CPU_DESCRIPTOR_HANDLE(_base.CpuHandle, _usedDescriptors, _descriptorHandleIncrementSize),
    CD3DX12_GPU_DESCRIPTOR_HANDLE(_base.GpuHandle, _usedDescriptors, _descriptorHandleIncrementSize));
    result.DescriptorHeap = _descriptorHeap;
    _usedDescriptors += count;

    return result;
}

bool DxDescriptorPage::HaveEnoughSpace(uint32 count) const {
    return _maxNumDescriptors - _usedDescriptors > count;
}

void DxDescriptorPage::Reset() {
    _usedDescriptors = 0;
}

void DxFrameDescriptorAllocator::NewFrame(uint32 bufferedFrameId) {
    _mutex.lock();

    _currentFrame = bufferedFrameId;
    while (not _usedPages[_currentFrame].empty()) {
        _freePages.push(std::move(_usedPages[_currentFrame].top()));
        _usedPages[_currentFrame].pop();
    }

    _mutex.unlock();
}

DxDescriptorRange DxFrameDescriptorAllocator::AllocateContiguousDescriptorRange(uint32 count) {
    _mutex.lock();

    DxDescriptorPage* current = nullptr;
    if (not _usedPages[_currentFrame].empty()) {
        current = _usedPages[_currentFrame].top().get();
    }

    if (not current or not current->HaveEnoughSpace(count)) {
        std::unique_ptr<DxDescriptorPage> freePage = nullptr;
        if (not _freePages.empty()) {
            freePage = std::move(_freePages.top());
            _freePages.pop();
        }
        if (not freePage) {
            freePage = std::make_unique<DxDescriptorPage>(1024);
            freePage->Init(DxContext::Instance().GetDevice());
        }

        freePage->Reset();
        current = freePage.get();
        _usedPages[_currentFrame].push(std::move(freePage));
    }

    DxDescriptorRange result = current->GetRange(count);

    _mutex.unlock();

    return result;
}

void DxPushableResourceDescriptorHeap::Inititalize(uint32 maxSize, bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = maxSize;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    if (shaderVisible) {
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }

    ThrowIfFailed(DxContext::Instance().GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(DescriptorHeap.GetAddressOf())));

    CurrentCPU = DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    CurrentGpu = DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

DxCpuDescriptorHandle DxPushableResourceDescriptorHeap::Push() {
    ++CurrentCPU;
    return CurrentCPU;
}

void CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC& desc, Com<ID3D12DescriptorHeap> &heap) {
    ThrowIfFailed(DxContext::Instance().GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.GetAddressOf())));
}

uint32 GetIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) {
    return DxContext::Instance().GetDevice()->GetDescriptorHandleIncrementSize(type);
}
