#include "DxDescriptorHeap.h"
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

	ThrowIfFailed(dxContext.GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&DescriptorHeap)));

	Type = type;
	_maxNumDescriptors = numDescriptors;
	_descriptorHandleIncrementSize = dxContext.GetDevice()->GetDescriptorHandleIncrementSize(type);
	_base.CpuHandle = DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	if (shaderVisible) {
		_base.GpuHandle = DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}
}

void DxDescriptorHeap::SetBase(DxDescriptorHandle handle) {
	_base.CpuHandle = handle.CpuHandle;
	_base.GpuHandle = handle.GpuHandle;
}

DxCbvSrvUavDescriptorHeap DxCbvSrvUavDescriptorHeap::Create(uint32 numDescriptors, bool shaderVisible) {
	DxCbvSrvUavDescriptorHeap descriptorHeap;
	descriptorHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numDescriptors, shaderVisible);
	return descriptorHeap;
}

DxDescriptorHandle DxDescriptorRange::PushEmptyHandle() {
	return GetHandle(_pushIndex++);
}

DxRtvDescriptorHeap DxRtvDescriptorHeap::CreateRTVDescriptorAllocator(uint32 numDescriptors) {
	DxRtvDescriptorHeap descriptorHeap;
	descriptorHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors, false);
	return descriptorHeap;
}

DxDsvDescriptorHeap DxDsvDescriptorHeap::CreateDSVDescriptorAllocator(uint32 numDescriptors) {
	DxDsvDescriptorHeap descriptorHeap;
	descriptorHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors, false);
	return descriptorHeap;
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureSRV(DxTexture& texture, DxDescriptorHandle handle, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureSRV(GetDevice(DescriptorHeap), handle, mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureSRV(DxTexture& texture, uint32 index, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureSRV(GetDevice(DescriptorHeap), GetHandle(index), mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Push2DTextureSRV(DxTexture& texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
	return Create2DTextureSRV(texture, _pushIndex++, mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateCubemapSRV(DxTexture& texture, DxDescriptorHandle handle, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
	return texture.CreateCubemapSRV(GetDevice(DescriptorHeap), handle, mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateCubemapSRV(DxTexture& texture, uint32 index, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
	return texture.CreateCubemapSRV(GetDevice(DescriptorHeap), GetHandle(index), mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::PushCubemapSRV(DxTexture& texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
	return CreateCubemapSRV(texture, _pushIndex++, mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateCubemapArraySRV(DxTexture& texture, DxDescriptorHandle handle, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
	return 	texture.CreateCubemapArraySRV(GetDevice(DescriptorHeap), handle, mipRange, firstCube, numCubes, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateCubemapArraySRV(DxTexture& texture, uint32 index, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
	return texture.CreateCubemapArraySRV(GetDevice(DescriptorHeap), GetHandle(index), mipRange, firstCube, numCubes, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::PushCubemapArraySRV(DxTexture& texture, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
	return CreateCubemapArraySRV(texture, _pushIndex++, mipRange, firstCube, numCubes, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateDepthTextureSRV(DxTexture& texture, DxDescriptorHandle handle) {
	return texture.CreateDepthTextureSRV(GetDevice(DescriptorHeap), handle);
}

DxDescriptorHandle DxDescriptorRange::CreateDepthTextureSRV(DxTexture& texture, uint32 index) {
	return texture.CreateDepthTextureSRV(GetDevice(DescriptorHeap), GetHandle(index));
}

DxDescriptorHandle DxDescriptorRange::PushDepthTextureSRV(DxTexture& texture) {
	return CreateDepthTextureSRV(texture, _pushIndex++);
}

DxDescriptorHandle DxDescriptorRange::CreateNullTextureSRV(DxDescriptorHandle handle) {
	return DxTexture::CreateNullSRV(GetDevice(DescriptorHeap), handle);
}

DxDescriptorHandle DxDescriptorRange::CreateNullTextureSRV(uint32 index) {
	return DxTexture::CreateNullSRV(GetDevice(DescriptorHeap), GetHandle(index));
}

DxDescriptorHandle DxDescriptorRange::PushNullTextureSRV() {
	return CreateNullTextureSRV(_pushIndex++);
}

DxDescriptorHandle DxDescriptorRange::CreateBufferSRV(DxBuffer& buffer, DxDescriptorHandle handle, BufferRange bufferRange) {
	return buffer.CreateSRV(GetDevice(DescriptorHeap), handle, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::CreateBufferSRV(DxBuffer& buffer, uint32 index, BufferRange bufferRange) {
	return buffer.CreateSRV(GetDevice(DescriptorHeap), GetHandle(index), bufferRange);
}

DxDescriptorHandle DxDescriptorRange::PushBufferSRV(DxBuffer& buffer, BufferRange bufferRange) {
	return CreateBufferSRV(buffer, _pushIndex++, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::CreateRawBufferSRV(DxBuffer& buffer, DxDescriptorHandle handle, BufferRange bufferRange) {
	return buffer.CreateRawSRV(GetDevice(DescriptorHeap), handle, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::CreateRawBufferSRV(DxBuffer& buffer, uint32 index, BufferRange bufferRange) {
	return buffer.CreateRawSRV(GetDevice(DescriptorHeap), GetHandle(index), bufferRange);
}

DxDescriptorHandle DxDescriptorRange::PushRawBufferSRV(DxBuffer& buffer, BufferRange bufferRange) {
	return CreateRawBufferSRV(buffer, _pushIndex++, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureUAV(DxTexture& texture, DxDescriptorHandle handle, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureUAV(GetDevice(DescriptorHeap), handle, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureUAV(DxTexture& texture, uint32 index, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureUAV(GetDevice(DescriptorHeap), GetHandle(index), overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Push2DTextureUAV(DxTexture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return Create2DTextureUAV(texture, _pushIndex++, mipSlice, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureArrayUAV(DxTexture& texture, DxDescriptorHandle handle, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureArrayUAV(GetDevice(DescriptorHeap), handle, mipSlice, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureArrayUAV(DxTexture& texture, uint32 index, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureArrayUAV(GetDevice(DescriptorHeap), GetHandle(index), mipSlice, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Push2DTextureArrayUAV(DxTexture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return Create2DTextureArrayUAV(texture, _pushIndex++, mipSlice, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateNullTextureUAV(DxDescriptorHandle handle) {
	return DxTexture::CreateNullSRV(GetDevice(DescriptorHeap), handle);
}

DxDescriptorHandle DxDescriptorRange::CreateNullTextureUAV(uint32 index) {
	return DxTexture::CreateNullSRV(GetDevice(DescriptorHeap), GetHandle(index));
}

DxDescriptorHandle DxDescriptorRange::PushNullTextureUAV() {
	return CreateNullTextureUAV(_pushIndex++);
}

DxDescriptorHandle DxDescriptorRange::CreateBufferUAV(DxBuffer& buffer, DxDescriptorHandle handle, BufferRange bufferRange) {
	return buffer.CreateUAV(GetDevice(DescriptorHeap), handle, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::CreateBufferUAV(DxBuffer& buffer, uint32 index, BufferRange bufferRange) {
	return buffer.CreateUAV(GetDevice(DescriptorHeap), GetHandle(index), bufferRange);
}

DxDescriptorHandle DxDescriptorRange::PushBufferUAV(DxBuffer& buffer, BufferRange bufferRange) {
	return CreateBufferUAV(buffer, _pushIndex++, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::CreateRaytracingAccelerationStructureSRV(DxBuffer& tlas, DxDescriptorHandle handle) {
	return tlas.CreateRaytracingAccelerationStructureSRV(GetDevice(DescriptorHeap), handle);
}

DxDescriptorHandle DxDescriptorRange::CreateRaytracingAccelerationStructureSRV(DxBuffer& tlas, uint32 index) {
	return tlas.CreateRaytracingAccelerationStructureSRV(GetDevice(DescriptorHeap), GetHandle(index));
}

DxDescriptorHandle DxDescriptorRange::PushRaytracingAccelerationStructureSRV(DxBuffer& tlas) {
	return CreateRaytracingAccelerationStructureSRV(tlas, _pushIndex++);
}

DxDescriptorHandle DxRtvDescriptorHeap::PushRenderTargetView(DxTexture* texture) {
	D3D12_RESOURCE_DESC resourceDesc = texture->Resource->GetDesc();
	assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	uint32 slices = resourceDesc.DepthOrArraySize;

	uint32 index = AtomicAdd(_pushIndex, slices);
	DxDescriptorHandle result = GetHandle(index);
	return CreateRenderTargetView(texture, result);
}

DxDescriptorHandle DxRtvDescriptorHeap::CreateRenderTargetView(DxTexture* texture, DxDescriptorHandle index) {
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
			GetDevice(DescriptorHeap)->CreateRenderTargetView(texture->Resource.Get(), &rtvDesc, rtv);
		}
	}
	else {
		GetDevice(DescriptorHeap)->CreateRenderTargetView(texture->Resource.Get(), nullptr, index.CpuHandle);
	}

	return index;
}

DxDescriptorHandle DxDsvDescriptorHeap::PushDepthStencilView(DxTexture* texture) {
	D3D12_RESOURCE_DESC resourceDesc = texture->Resource->GetDesc();
	assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	uint32 slices = resourceDesc.DepthOrArraySize;

	uint32 index = AtomicAdd(_pushIndex, slices);
	DxDescriptorHandle result = GetHandle(index);
	return CreateDepthStencilView(texture, result);
}

DxDescriptorHandle DxDsvDescriptorHeap::CreateDepthStencilView(DxTexture* texture, DxDescriptorHandle index) {
	D3D12_RESOURCE_DESC resourceDesc = texture->Resource->GetDesc();
	DXGI_FORMAT format = resourceDesc.Format;
	if (IsDepthFormat(format)) {
		resourceDesc.Format = GetTypelessFormat(format);
	}

	assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
	assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	if (IsDepthFormat(format)) {
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = format;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		GetDevice(DescriptorHeap)->CreateDepthStencilView(texture->Resource.Get(), &dsvDesc, index.CpuHandle);
	}
	else {
		GetDevice(DescriptorHeap)->CreateDepthStencilView(texture->Resource.Get(), nullptr, index.CpuHandle);
	}

	return index;
}

void DxFrameDescriptorAllocator::NewFrame(uint32 bufferedFrameId) {
	_mutex.lock();

	_currentFrame = bufferedFrameId;

	while (_usedPages[_currentFrame]) {
		DxDescriptorPage* page = _usedPages[_currentFrame];
		_usedPages[_currentFrame] = page->Next;
		page->Next = _freePages;
		_freePages = page;
	}

	_mutex.unlock();
}

DxDescriptorRange DxFrameDescriptorAllocator::AllocateContiguousDescriptorRange(uint32 count) {
	_mutex.lock();

	DxDescriptorPage* current = _usedPages[_currentFrame];
	if (!current or (current->MaxNumDescriptors - current->UsedDescriptors < count)) {
		DxDescriptorPage* freePage = _freePages;
		if (not freePage) {
			freePage = (DxDescriptorPage*)calloc(1, sizeof(DxDescriptorPage));

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = 1024;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			ThrowIfFailed(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(freePage->DescriptorHeap.GetAddressOf())));

			freePage->DescriptorHandleIncrementSize = _device->GetDescriptorHandleIncrementSize(desc.Type);
			freePage->Base.CpuHandle = freePage->DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			freePage->Base.GpuHandle = freePage->DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		}

		freePage->UsedDescriptors = 0;
		freePage->Next = current;
		_usedPages[_currentFrame] = freePage;
		current = freePage;
	}

	uint32 index = current->UsedDescriptors;
	current->UsedDescriptors += count;

	_mutex.unlock();

	DxDescriptorRange result(current->MaxNumDescriptors, current->DescriptorHandleIncrementSize);
	result.SetBase({
		CD3DX12_CPU_DESCRIPTOR_HANDLE(current->Base.CpuHandle, index, current->DescriptorHandleIncrementSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(current->Base.GpuHandle, index, current->DescriptorHandleIncrementSize)
	});
	result.DescriptorHeap = current->DescriptorHeap;

	return result;
}