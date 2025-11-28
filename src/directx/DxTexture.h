#pragma once

#include <mutex>
#include <unordered_map>

#include "../directx/dx.h"
#include "DxDescriptor.h"


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
	DxTexture() = default;
	DxTexture(DxResource resource, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = {});
	virtual ~DxTexture();

	void SetName(const wchar* name);
	std::wstring GetName() const;

	void Resize(uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState = (D3D12_RESOURCE_STATES)-1);
	void AllocateMipUAVs();

	ID3D12Resource* Resource() const { return _resource.Get(); }
	void SetResource(const DxResource &resource) { _resource = resource; }
	const DxCpuDescriptorHandle& DefaultSRV() const  { return _defaultSRV; }
	const DxCpuDescriptorHandle& DefaultUAV() const { return _defaultUAV; }
	const DxCpuDescriptorHandle& StencilSRV() const { return _stencilSRV; }
	const DxRtvDescriptorHandle& RTVHandles() const { return _rtvHandles; }
	const DxDsvDescriptorHandle& DSVHandles() const { return _dsvHandle; }
	DXGI_FORMAT Format() const { return _format; }
	bool SupportsRTV() const { return _supportsRTV; }
	bool SupportsDSV() const { return _supportsDSV; }
	bool SupportsUAV() const { return _supportsUAV; }
	bool SupportsSRV() const { return _supportsSRV; }
	uint32 NumMipLevels() const { return _numMipLevels; }
	const std::vector<DxCpuDescriptorHandle>& MipUAVs() const { return _mipUAVs; }
	std::vector<DxCpuDescriptorHandle>& MipUAVs() { return _mipUAVs; }

	uint32 Width() const { return _width; }
	uint32 Height() const { return _height; }
	uint32 Depth() const { return _depth; }

	static bool IsUAVCompatibleFormat(DXGI_FORMAT format);
	static bool IsSRGBFormat(DXGI_FORMAT format);
	static bool IsBGRFormat(DXGI_FORMAT format);
	static bool IsDepthFormat(DXGI_FORMAT format);
	static bool IsStencilFormat(DXGI_FORMAT format);
	static DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT format);
	static DXGI_FORMAT GetUAVCompatibleFormat(DXGI_FORMAT format);
	static DXGI_FORMAT GetDepthReadFormat(DXGI_FORMAT format);
	static DXGI_FORMAT GetStencilReadFormat(DXGI_FORMAT format);
	static uint32 GetFormatSize(DXGI_FORMAT format);
	static DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
	static uint32 GetNumberOfChannels(DXGI_FORMAT format);

private:
	void UploadSubresourceData(D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources);
	void Update(const char* filename, uint32 flags);

	DxResource _resource;

	uint32 _width, _height, _depth;
	DXGI_FORMAT _format;
	D3D12_FEATURE_DATA_FORMAT_SUPPORT _formatSupport;
	D3D12_RESOURCE_STATES _initialState;
	uint32 _requestedNumMipLevels;
	uint32 _numMipLevels;

	DxCpuDescriptorHandle _defaultSRV; // SRV for the whole texture (all mip levels).
	DxCpuDescriptorHandle _defaultUAV; // UAV for the first mip level
	DxCpuDescriptorHandle _stencilSRV; // For depth stencil textures.

	DxRtvDescriptorHandle _rtvHandles;
	DxDsvDescriptorHandle _dsvHandle;

	std::vector<DxCpuDescriptorHandle> _mipUAVs;

	bool _supportsRTV;
	bool _supportsDSV;
	bool _supportsUAV;
	bool _supportsSRV;

	friend class DxDescriptorRange;
	friend class TextureFactory;
};

struct TextureGrave {
	DxResource Resource;

	DxCpuDescriptorHandle Srv;
	DxCpuDescriptorHandle Uav;
	DxCpuDescriptorHandle Stencil;
	DxRtvDescriptorHandle Rtv;
	DxDsvDescriptorHandle Dsv;

	std::vector<DxCpuDescriptorHandle> MipUAVs;

	TextureGrave() {}
	TextureGrave(const TextureGrave&) = delete;
	TextureGrave(TextureGrave&&) = default;

	TextureGrave& operator=(const TextureGrave&) = delete;
	TextureGrave& operator=(TextureGrave&&) = default;

	~TextureGrave();
};

class TextureFactory {
public:
	static TextureFactory* Instance() { return _instance; }

	Ptr<DxTexture> CreateTexture(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA *subresourceData,
						uint32 numSubresources, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	Ptr<DxTexture> CreateTexture(const void *data, uint32 width, uint32 height, DXGI_FORMAT format,
						 bool allocateMips = false, bool allowRenderTarget = false,  bool allowUnorderedAccess = false,
						 D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	Ptr<DxTexture> CreateDepthTexture(uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength = 1,
							D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
	Ptr<DxTexture> CreateCubeTexture(const void* data, uint32 width, uint32 height, uint32 depth, DXGI_FORMAT format, bool allocateMips = false, bool allowRenderTarget = false, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	Ptr<DxTexture> CreateVolumeTexture(const void* data, uint32 width, uint32 height, uint32 depth, DXGI_FORMAT format, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

	Ptr<DxTexture> LoadTextureFromFile(const char *filename, uint32 flags = ETextureLoadFlagsDefault);
	Ptr<DxTexture> LoadVolumeTextureFromDirectory(const char* dirname, uint32 flags = ETextureLoadFlagsCompress | ETextureLoadFlagsCacheToDds | ETextureLoadFlagsNoncolor);


private:
	static TextureFactory* _instance;
	static Ptr<DxTexture> LoadVolumeTextureInternal(const std::string& dirname, uint32 flags);
	static Ptr<DxTexture> LoadTextureInternal(const std::string& filename, uint32 flags);

	std::unordered_map<std::string, WeakPtr<DxTexture>> _textureCache; // TODO: Pack flags into key.
	std::mutex _mutex;
};