#pragma once

#include "../directx/dx.h"
#include "DxDescriptorHeap.h"
#include "DxContext.h"

// If the TextureLoadFlags flags is set, the system will cache the texture as DDS to disk for faster loading next time.
// This is not done if the original file has a newer write time.
// It is also not done if the cache was created with different flags.
// Therefore: If you change these flags, delete the texture cache!

// If you want the mip chain to be computed on the GPU, you must call this yourself. This system only supports CPU mip levels for now.

enum TextureLoadFlags {
	ETextureLoadFlagsNone					= 0,
	ETextureLoadFlagsNoncolor				= (1 << 0),
	ETextureLoadFlagsCompress				= (1 << 1),
	ETextureLoadFlagsGenMipsOnCpu			= (1 << 2),
	ETextureLoadFlagsGenMipsOnGpu			= (1 << 3),
	ETextureLoadFlagsAllocateFullMipChain	= (1 << 4), // Use if you want to create the mip chain on the GPU
	ETextureLoadFlagsPremultiplyAlpha		= (1 << 5),
	ETextureLoadFlagsCacheToDds				= (1 << 6),
	ETextureLoadFlagsAlwaysLoadFromSource	= (1 << 7), // By default the system will always try to load a cached version of the texture. You can prevent this with this flag.

	ETextureLoadFlagsDefault = ETextureLoadFlagsCompress | ETextureLoadFlagsGenMipsOnCpu | ETextureLoadFlagsCacheToDds,
};

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

	static DxTexture LoadFromFile(const char* filename, uint32 flags = ETextureLoadFlagsDefault);
private:
	void UploadSubresourceData(D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources);
	void Update(const char* filename, uint32 flags);

	DxDescriptorHandle Create2DTextureSRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;
	DxDescriptorHandle CreateCubemapSRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;
	DxDescriptorHandle CreateCubemapArraySRV(DxDevice device, DxDescriptorHandle index, TextureMipRange mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;
	DxDescriptorHandle CreateDepthTextureSRV(DxDevice device, DxDescriptorHandle index) const;
	static DxDescriptorHandle CreateNullSRV(DxDevice device, DxDescriptorHandle index);
	DxDescriptorHandle Create2DTextureUAV(DxDevice device, DxDescriptorHandle index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;
	DxDescriptorHandle Create2DTextureArrayUAV(DxDevice device, DxDescriptorHandle index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN) const;

	friend DxDescriptorRange;
};