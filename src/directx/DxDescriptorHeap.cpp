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

	ThrowIfFailed(dxContext.Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(DescriptorHeap.GetAddressOf())));

	Type = type;
	MaxNumDescriptors = numDescriptors;
	DescriptorHandleIncrementSize = dxContext.Device->GetDescriptorHandleIncrementSize(type);
	Base.CpuHandle = DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	Base.GpuHandle = DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

DxCbvSrvUavDescriptorHeap DxCbvSrvUavDescriptorHeap::Create(uint32 numDescriptors, bool shaderVisible) {
	DxCbvSrvUavDescriptorHeap descriptorHeap;
	descriptorHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numDescriptors, shaderVisible);
	descriptorHeap.PushIndex = 0;
	return descriptorHeap;
}

DxRtvDescriptorHeap DxRtvDescriptorHeap::CreateRTVDescriptorAllocator(uint32 numDescriptors) {
	DxRtvDescriptorHeap descriptorHeap;
	descriptorHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors, false);
	descriptorHeap.PushIndex = 0;
	return descriptorHeap;
}

DxDsvDescriptorHeap DxDsvDescriptorHeap::CreateDSVDescriptorAllocator(uint32 numDescriptors) {
	DxDsvDescriptorHeap descriptorHeap;
	descriptorHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors, false);
	descriptorHeap.PushIndex = 0;
	return descriptorHeap;
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureSRV(DxTexture& texture, uint32 index, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureSRV(GetDevice(DescriptorHeap), GetHandle(index), mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Push2DTextureSRV(DxTexture& texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
	return Create2DTextureSRV(texture, PushIndex++, mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateCubemapSRV(DxTexture& texture, uint32 index, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
	return texture.CreateCubemapSRV(GetDevice(DescriptorHeap), GetHandle(index), mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::PushCubemapSRV(DxTexture& texture, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) {
	return CreateCubemapSRV(texture, PushIndex++, mipRange, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateCubemapArraySRV(DxTexture& texture, uint32 index, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
	return texture.CreateCubemapArraySRV(GetDevice(DescriptorHeap), GetHandle(index), mipRange, firstCube, numCubes, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::PushCubemapArraySRV(DxTexture& texture, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) {
	return CreateCubemapArraySRV(texture, PushIndex++, mipRange, firstCube, numCubes, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateDepthTextureSRV(DxTexture& texture, uint32 index) {
	return texture.CreateDepthTextureSRV(GetDevice(DescriptorHeap), GetHandle(index));
}

DxDescriptorHandle DxDescriptorRange::PushDepthTextureSRV(DxTexture& texture) {
	return CreateDepthTextureSRV(texture, PushIndex++);
}

DxDescriptorHandle DxDescriptorRange::CreateNullTextureSRV(uint32 index) {
	return DxTexture::CreateNullSRV(GetDevice(DescriptorHeap), GetHandle(index));
}

DxDescriptorHandle DxDescriptorRange::PushNullTextureSRV() {
	return CreateNullTextureSRV(PushIndex++);
}

DxDescriptorHandle DxDescriptorRange::CreateBufferSRV(DxBuffer& buffer, uint32 index, BufferRange bufferRange) {
	return buffer.CreateSRV(GetDevice(DescriptorHeap), GetHandle(index), bufferRange);
}

DxDescriptorHandle DxDescriptorRange::PushBufferSRV(DxBuffer& buffer, BufferRange bufferRange) {
	return CreateBufferSRV(buffer, PushIndex++, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::CreateRawBufferSRV(DxBuffer& buffer, uint32 index, BufferRange bufferRange) {
	return buffer.CreateRawSRV(GetDevice(DescriptorHeap), GetHandle(index), bufferRange);
}

DxDescriptorHandle DxDescriptorRange::PushRawBufferSRV(DxBuffer& buffer, BufferRange bufferRange) {
	return CreateRawBufferSRV(buffer, PushIndex++, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureUAV(DxTexture& texture, uint32 index, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureUAV(GetDevice(DescriptorHeap), GetHandle(index), overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Push2DTextureUAV(DxTexture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return Create2DTextureUAV(texture, PushIndex++, mipSlice, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Create2DTextureArrayUAV(DxTexture& texture, uint32 index, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return texture.Create2DTextureArrayUAV(GetDevice(DescriptorHeap), GetHandle(index), mipSlice, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::Push2DTextureArrayUAV(DxTexture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat) {
	return Create2DTextureArrayUAV(texture, PushIndex++, mipSlice, overrideFormat);
}

DxDescriptorHandle DxDescriptorRange::CreateNullTextureUAV(uint32 index) {
	return DxTexture::CreateNullSRV(GetDevice(DescriptorHeap), GetHandle(index));
}

DxDescriptorHandle DxDescriptorRange::PushNullTextureUAV() {
	return CreateNullTextureUAV(PushIndex++);
}

DxDescriptorHandle DxDescriptorRange::CreateBufferUAV(DxBuffer& buffer, uint32 index, BufferRange bufferRange) {
	return buffer.CreateUAV(GetDevice(DescriptorHeap), GetHandle(index), bufferRange);
}

DxDescriptorHandle DxDescriptorRange::PushBufferUAV(DxBuffer& buffer, BufferRange bufferRange) {
	return CreateBufferUAV(buffer, PushIndex++, bufferRange);
}

DxDescriptorHandle DxDescriptorRange::CreateRaytracingAccelerationStructureSRV(DxBuffer& tlas, uint32 index) {
	return tlas.CreateRaytracingAccelerationStructureSRV(GetDevice(DescriptorHeap), GetHandle(index));
}

DxDescriptorHandle DxDescriptorRange::PushRaytracingAccelerationStructureSRV(DxBuffer& tlas) {
	return CreateRaytracingAccelerationStructureSRV(tlas, PushIndex++);
}

DxDescriptorHandle DxRtvDescriptorHeap::PushRenderTargetView(DxTexture* texture) {
	D3D12_RESOURCE_DESC resourceDesc = texture->Resource->GetDesc();
	assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	uint32 slices = resourceDesc.DepthOrArraySize;

	uint32 index = AtomicAdd(PushIndex, slices);
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
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(index.CpuHandle, i, DescriptorHandleIncrementSize);
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

	uint32 index = AtomicAdd(PushIndex, slices);
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

DxFrameDescriptorAllocator DxFrameDescriptorAllocator::Create() {
	DxFrameDescriptorAllocator result = {};
	result.Device = DxContext::Instance().Device;
	result.Mutex = ThreadMutex::Create();
	result.CurrentFrame = NUM_BUFFERED_FRAMES - 1;
	return result;
}

void DxFrameDescriptorAllocator::NewFrame(uint32 bufferedFrameId) {
	Mutex.Lock();

	CurrentFrame = bufferedFrameId;

	while (UsedPages[CurrentFrame]) {
		DxDescriptorPage* page = UsedPages[CurrentFrame];
		UsedPages[CurrentFrame] = page->Next;
		page->Next = FreePages;
		FreePages = page;
	}

	Mutex.Unlock();
}

DxDescriptorRange DxFrameDescriptorAllocator::AllocateContiguousDescriptorRange(uint32 count) {
	Mutex.Lock();

	DxDescriptorPage* current = UsedPages[CurrentFrame];
	if (!current or (current->MaxNumDescriptors - current->UsedDescriptors < count)) {
		DxDescriptorPage* freePage = FreePages;
		if (not freePage) {
			freePage = (DxDescriptorPage*)calloc(1, sizeof(DxDescriptorPage));

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = 1024;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			ThrowIfFailed(Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(freePage->DescriptorHeap.GetAddressOf())));

			freePage->DescriptorHandleIncrementSize = Device->GetDescriptorHandleIncrementSize(desc.Type);
			freePage->Base.CpuHandle = freePage->DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			freePage->Base.GpuHandle = freePage->DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		}

		freePage->UsedDescriptors = 0;
		freePage->Next = current;
		UsedPages[CurrentFrame] = freePage;
		current = freePage;
	}

	uint32 index = current->UsedDescriptors;
	current->UsedDescriptors += count;

	Mutex.Unlock();

	DxDescriptorRange result;

	result.Base.GpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(current->Base.GpuHandle, index, current->DescriptorHandleIncrementSize);
	result.Base.CpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(current->Base.CpuHandle, index, current->DescriptorHandleIncrementSize);
	result.DescriptorHandleIncrementSize = current->DescriptorHandleIncrementSize;
	result.DescriptorHeap = current->DescriptorHeap;
	result.MaxNumDescriptors = count;
	result.PushIndex = 0;

	return result;
}