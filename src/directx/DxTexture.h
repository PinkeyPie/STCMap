#pragma once

#include "../directx/dx.h"
#include "DxDescriptorHeap.h"
#include "DxContext.h"

class DxTexture {
public:
	DxResource Resource;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport;

	DxDescriptorHandle RTVHandles;
	DxDescriptorHandle DSVHandle;

	void Resize(uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

	static DxTexture Create(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	static DxTexture Create(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allowRenderTarget = false, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	static DxTexture CreateDepth(uint32 width, uint32 height, DXGI_FORMAT format);

	bool FormatSupportsRTV() const;
	bool FormatSupportsDSV() const;
	bool FormatSupportsSRV() const;
	bool FormatSupportsUAV() const;
private:
	void UploadSubresourceData(D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources);
	DxDescriptorHandle Create2DTextureSRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;
	DxDescriptorHandle CreateCubemapSRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;
	DxDescriptorHandle CreateCubemapArraySRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;
	DxDescriptorHandle CreateDepthTextureSRV(DxDevice device, DxDescriptorHandle index) const;
	static DxDescriptorHandle CreateNullSRV(DxDevice device, DxDescriptorHandle index);
	DxDescriptorHandle Create2DTextureUAV(DxDevice device, DxDescriptorHandle index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;
	DxDescriptorHandle Create2DTextureArrayUAV(DxDevice device, DxDescriptorHandle index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;

	friend DxDescriptorRange;
};