#include "DxDescriptorAllocation.h"
#include "DxTexture.h"
#include "DxBuffer.h"
#include "DxRenderPrimitives.h"


void DxDescriptorHeap::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible) {
    DxContext& dxContext = DxContext::Instance();
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;
    if (shaderVisible) {
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }

    ThrowIfFailed(dxContext.GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_descriptorHeap)));

    _allFreeIncludingAndAfter = 0;
    _descriptorHandleIncrementSize = dxContext.GetDevice()->GetDescriptorHandleIncrementSize(type);
    _cpuBase = _descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    _gpuBase = _descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    Type = type;
}

DxCpuDescriptorHandle DxDescriptorHeap::GetFreeHandle() {
    uint32 index;
    if (!_freeDescriptors.empty()) {
        index = _freeDescriptors.back();
        _freeDescriptors.pop_back();
    }
    else {
        index = _allFreeIncludingAndAfter++;
    }
    return { CD3DX12_CPU_DESCRIPTOR_HANDLE(_cpuBase, index, _descriptorHandleIncrementSize)};
}

void DxDescriptorHeap::FreeHandle(DxCpuDescriptorHandle handle) {
    uint32 index = (uint32)((handle.CpuHandle.ptr - _cpuBase.ptr) / _descriptorHandleIncrementSize);
    _freeDescriptors.push_back(index);
}

DxCpuDescriptorHandle DxDescriptorHeap::GetMatchingCpuHandle(DxGpuDescriptorHandle handle) {
    uint32 offset = (uint32)(handle.GpuHandle.ptr - _gpuBase.ptr);
    return { CD3DX12_CPU_DESCRIPTOR_HANDLE(_cpuBase, offset)};
}

DxGpuDescriptorHandle DxDescriptorHeap::GetMatchingGpuHandle(DxCpuDescriptorHandle handle) {
    uint32 offset = (uint32)(handle.CpuHandle.ptr - _cpuBase.ptr);
    return {CD3DX12_GPU_DESCRIPTOR_HANDLE(_gpuBase, offset)};
}

void DxRtvDescriptorHeap::Initialize(uint32 numDescriptors, bool shaderVisible) {
    _pushIndex = 0;
    DxDescriptorHeap::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors, shaderVisible);
}

DxCpuDescriptorHandle DxRtvDescriptorHeap::PushRenderTargetView(DxTexture *texture) {
    D3D12_RESOURCE_DESC resourceDesc = texture->Resource->GetDesc();
    assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    uint32 slices = resourceDesc.DepthOrArraySize;

    uint32 index = AtomicAdd(_pushIndex, slices);
    DxCpuDescriptorHandle result = GetHandle(index);
    return CreateRenderTargetView(texture, result);
}

DxCpuDescriptorHandle DxRtvDescriptorHeap::CreateRenderTargetView(DxTexture *texture, DxCpuDescriptorHandle index) {
    D3D12_RESOURCE_DESC resourceDesc = texture->Resource->GetDesc();
    assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    uint32 slices = resourceDesc.DepthOrArraySize;

    if (slices > 1) {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Format = resourceDesc.Format;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.PlaneSlice = 0;

        for (uint32 i = 0; i < slices; i++) {
            rtvDesc.Texture2DArray.FirstArraySlice = i;
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(index.CpuHandle, i, _descriptorHandleIncrementSize);
            GetDevice(_descriptorHeap)->CreateRenderTargetView(texture->Resource.Get(), &rtvDesc, rtv);
        }
    }
    else {
        GetDevice(_descriptorHeap)->CreateRenderTargetView(texture->Resource.Get(), nullptr, index.CpuHandle);
    }

    return index;
}

void DxDsvDescriptorHeap::Initialize(uint32 numDescriptors, bool shaderVisible) {
    _pushIndex = 0;
    DxDescriptorHeap::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors, shaderVisible);
}

DxCpuDescriptorHandle DxDsvDescriptorHeap::PushDepthStencilView(DxTexture *texture) {
    D3D12_RESOURCE_DESC resourceDesc = texture->Resource->GetDesc();
    assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    uint32 slices = resourceDesc.DepthOrArraySize;

    uint32 index = AtomicAdd(_pushIndex, slices);
    DxCpuDescriptorHandle result = GetHandle(index);
    return CreateDepthStencilView(texture, result);
}

DxCpuDescriptorHandle DxDsvDescriptorHeap::CreateDepthStencilView(DxTexture *texture, DxCpuDescriptorHandle index) {
    DXGI_FORMAT format = texture->FormatSupport.Format;

    assert(IsDepthFormat(format));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    GetDevice(_descriptorHeap)->CreateDepthStencilView(texture->Resource.Get(), &dsvDesc, index.CpuHandle);

    return index;
}

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
        DxDescriptorPage* page = _usedPages[_currentFrame].top();
        _usedPages[_currentFrame].pop();
        _freePages.push(page);
    }

    _mutex.unlock();
}

DxDescriptorRange DxFrameDescriptorAllocator::AllocateContiguousDescriptorRange(uint32 count) {
    _mutex.lock();

    DxDescriptorPage* current = nullptr;
    if (not _usedPages[_currentFrame].empty()) {
        current = _usedPages[_currentFrame].top();
    }

    if (not current or not current->HaveEnoughSpace(count)) {
        DxDescriptorPage* freePage = nullptr;
        if (not _freePages.empty()) {
            freePage = _freePages.top();
            _freePages.pop();
        }
        if (not freePage) {
            freePage = new DxDescriptorPage{1024};
            freePage->Init(_device.Get());
        }

        freePage->Reset();
        _usedPages[_currentFrame].push(freePage);
        current = freePage;
    }

    DxDescriptorRange result = current->GetRange(count);

    _mutex.unlock();

    return result;
}
