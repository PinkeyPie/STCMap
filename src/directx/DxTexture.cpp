#include "DxTexture.h"
#include "DxRenderPrimitives.h"

void DxTexture::UploadSubresourceData(D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources) {
	DxContext& dxContext = DxContext::Instance();
	DxCommandList* commandList = dxContext.GetFreeCopyCommandList();
	commandList->TransitionBarrier(Resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	uint64 requiredSize = GetRequiredIntermediateSize(Resource.Get(), firstSubresource, numSubresources);

	DxResource intermediateResource;
	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
	ThrowIfFailed(dxContext.Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(intermediateResource.GetAddressOf())
	));

	UpdateSubresources<128>(commandList->CommandList.Get(), Resource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);
	dxContext.RetireObject(intermediateResource);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.
	// commandList->TransitionBarrier(Resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

	dxContext.ExecuteCommandList(commandList);
}

DxTexture DxTexture::Create(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState) {
	DxContext& dxContext = DxContext::Instance();
	DxTexture result;

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(dxContext.Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(result.Resource.GetAddressOf())
	));

	result.FormatSupport.Format = textureDesc.Format;
	ThrowIfFailed(dxContext.Device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&result.FormatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)
	));

	if ((textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 and result.FormatSupportsRTV()) {
		result.RTVHandles = dxContext.RtvAllocator.PushRenderTargetView(&result);
	}

	if ((textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 9 and (result.FormatSupportsDSV() or IsDepthFormat(textureDesc.Format))) {
		result.DSVHandle = dxContext.DsvAllocator.PushDepthStencilView(&result);
	}

	if (subresourceData) {
		result.UploadSubresourceData(subresourceData, 0, numSubresources);
	}

	return result;
}

DxTexture DxTexture::Create(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState) {
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE)
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE);

	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, flags);

	uint32 formatSize = GetFormatSize(textureDesc.Format);

	if (data) {
		D3D12_SUBRESOURCE_DATA subresource;
		subresource.RowPitch = width * formatSize;
		subresource.SlicePitch = width * height * formatSize;
		subresource.pData = data;

		return Create(textureDesc, &subresource, 1, initialState);
	}
	else {
		return Create(textureDesc, nullptr, 0, initialState);
	}
}

DxTexture DxTexture::CreateDepth(uint32 width, uint32 height, DXGI_FORMAT format) {
	DxContext& dxContext = DxContext::Instance();
	DxTexture result;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.DepthStencil = { 1.f, 0 };

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 
		1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(dxContext.Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(result.Resource.GetAddressOf())
	));

	result.FormatSupport.Format = format;
	ThrowIfFailed(dxContext.Device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&result.FormatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)
	));

	result.DSVHandle = dxContext.DsvAllocator.PushDepthStencilView(&result);

	return result;
}

void DxTexture::Resize(uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState) {
	DxContext& dxContext = DxContext::Instance();
	D3D12_RESOURCE_DESC desc = Resource->GetDesc();

	D3D12_RESOURCE_STATES state = initialState;
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	D3D12_CLEAR_VALUE* clearValue = nullptr;

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
		state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		optimizedClearValue.Format = desc.Format;
		optimizedClearValue.DepthStencil = { 1.f, 0 };
		clearValue = &optimizedClearValue;
	}

	dxContext.RetireObject(Resource);

	desc.Width = newWidth;
	desc.Height = newHeight;

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(dxContext.Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		state,
		clearValue,
		IID_PPV_ARGS(Resource.GetAddressOf())
	));

	if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 and FormatSupportsRTV()) {
		dxContext.RtvAllocator.CreateRenderTargetView(this, RTVHandles);
	}

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
		dxContext.DsvAllocator.CreateDepthStencilView(this, DSVHandle);
	}
}

bool DxTexture::FormatSupportsRTV() const {
	return CheckFormatSupport(FormatSupport, D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
}

bool DxTexture::FormatSupportsDSV() const {
	return CheckFormatSupport(FormatSupport, D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
}

bool DxTexture::FormatSupportsSRV() const {
	return CheckFormatSupport(FormatSupport, D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
}

bool DxTexture::FormatSupportsUAV() const {
	return CheckFormatSupport(FormatSupport, D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) and
		CheckFormatSupport(FormatSupport, D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) and
		CheckFormatSupport(FormatSupport, D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
}

DxDescriptorHandle DxTexture::Create2DTextureSRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? Resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = mipRange.First;
	srvDesc.Texture2D.MipLevels = mipRange.Count;

	device->CreateShaderResourceView(Resource.Get(), &srvDesc, index.CpuHandle);

	return index;
}

DxDescriptorHandle DxTexture::CreateCubemapSRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange, DXGI_FORMAT overrideFormat) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? Resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = mipRange.First;
	srvDesc.TextureCube.MipLevels = mipRange.Count;

	device->CreateShaderResourceView(Resource.Get(), &srvDesc, index.CpuHandle);

	return index;
}

DxDescriptorHandle DxTexture::CreateCubemapArraySRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? Resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
	srvDesc.TextureCubeArray.MostDetailedMip = mipRange.First;
	srvDesc.TextureCubeArray.MipLevels = mipRange.Count;
	srvDesc.TextureCubeArray.NumCubes = numCubes;
	srvDesc.TextureCubeArray.First2DArrayFace = firstCube * 6;

	device->CreateShaderResourceView(Resource.Get(), &srvDesc, index.CpuHandle);

	return index;
}

DxDescriptorHandle DxTexture::CreateDepthTextureSRV(DxDevice device, DxDescriptorHandle index) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = GetReadFormatFromTypeless(Resource->GetDesc().Format);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(Resource.Get(), &srvDesc, index.CpuHandle);

	return index;
}

DxDescriptorHandle DxTexture::CreateNullSRV(DxDevice device, DxDescriptorHandle index) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 0;

	device->CreateShaderResourceView(nullptr, &srvDesc, index.CpuHandle);

	return index;
}

DxDescriptorHandle DxTexture::Create2DTextureUAV(DxDevice device, DxDescriptorHandle index, uint32 mipSlice, DXGI_FORMAT overrideFormat) const {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2D.MipSlice = mipSlice;
	device->CreateUnorderedAccessView(Resource.Get(), nullptr, &uavDesc, index.CpuHandle);

	return index;
}

DxDescriptorHandle DxTexture::Create2DTextureArrayUAV(DxDevice device, DxDescriptorHandle index, uint32 mipSlice, DXGI_FORMAT overrideFormat) const {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = Resource->GetDesc().DepthOrArraySize;
	uavDesc.Texture2DArray.MipSlice = mipSlice;
	device->CreateUnorderedAccessView(Resource.Get(), nullptr, &uavDesc, index.CpuHandle);

	return index;
}
