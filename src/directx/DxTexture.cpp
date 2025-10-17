#include "DxTexture.h"
#include <filesystem>
#include "DxContext.h"
#include "DxRenderPrimitives.h"
#include "DirectXTex.h"

namespace fs = std::filesystem;

namespace {
	DXGI_FORMAT MakeSrgb(DXGI_FORMAT format) {
		return DirectX::MakeSRGB(format);
	}

	DXGI_FORMAT MakeLinear(DXGI_FORMAT format) {
		switch (format) {
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				format = DXGI_FORMAT_R8G8B8A8_UNORM;
				break;
			case DXGI_FORMAT_BC1_UNORM_SRGB:
				format = DXGI_FORMAT_BC1_UNORM;
				break;
			case DXGI_FORMAT_BC2_UNORM_SRGB:
				format = DXGI_FORMAT_BC2_UNORM;
				break;
			case DXGI_FORMAT_BC3_UNORM_SRGB:
				format = DXGI_FORMAT_BC3_UNORM;
				break;
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				format = DXGI_FORMAT_B8G8R8A8_UNORM;
				break;
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
				format = DXGI_FORMAT_B8G8R8X8_UNORM;
				break;
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				format = DXGI_FORMAT_BC7_UNORM;
				break;
		}

		return format;
	}

	bool LoadImageFromFile(const char* filepathRaw, uint32 flags, DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc) {
		fs::path filepath = filepathRaw;
		fs::path extension = filepath.extension();

		fs::path cachedFilename = filepath;
		cachedFilename.replace_extension("." + std::to_string(flags) + ".cache.dds");

		fs::path cacheFilepath = L"bin_cache" / cachedFilename;

		bool fromCache = false;
		DirectX::TexMetadata metadata;

		if (not(flags & ETextureLoadFlagsAlwaysLoadFromSource)) {
			// Look for cached

			WIN32_FILE_ATTRIBUTE_DATA cachedData;
			if (GetFileAttributesExW(cacheFilepath.c_str(), GetFileExInfoStandard, &cachedData)) {
				FILETIME cachedFiletime = cachedData.ftLastWriteTime;

				WIN32_FILE_ATTRIBUTE_DATA originalData;
				assert(GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &originalData));
				FILETIME originalFiletime = originalData.ftLastWriteTime;

				if (CompareFileTime(&cachedFiletime, &originalFiletime) >= 0) {
					// Cached file is newer than original, so load this
					fromCache = SUCCEEDED(DirectX::LoadFromDDSFile(cacheFilepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
				}
			}
		}

		if (not fromCache) {
			if (extension == ".dds") {
				ThrowIfFailed(DirectX::LoadFromDDSFile(filepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
			}
			else if (extension == ".hdr") {
				ThrowIfFailed(DirectX::LoadFromHDRFile(filepath.c_str(), &metadata, scratchImage));
			}
			else if (extension == ".tga") {
				ThrowIfFailed(DirectX::LoadFromTGAFile(filepath.c_str(), &metadata, scratchImage));
			}
			else {
				ThrowIfFailed(DirectX::LoadFromWICFile(filepath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage));
			}

			if (flags & ETextureLoadFlagsNoncolor) {
				metadata.format = MakeLinear(metadata.format);
			}
			else {
				metadata.format = MakeSrgb(metadata.format);
			}

			scratchImage.OverrideFormat(metadata.format);

			if (flags & ETextureLoadFlagsGenMipsOnCpu) {
				DirectX::ScratchImage mipChainImage;

				ThrowIfFailed(DirectX::GenerateMipMaps(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::TEX_FILTER_DEFAULT, 0, mipChainImage));
				scratchImage = std::move(mipChainImage);
				metadata = scratchImage.GetMetadata();
			}
			else {
				metadata.mipLevels = 1;
			}

			if (flags & ETextureLoadFlagsPremultiplyAlpha) {
				DirectX::ScratchImage premultipliedAlphaImage;

				ThrowIfFailed(DirectX::PremultiplyAlpha(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::TEX_PMALPHA_DEFAULT, premultipliedAlphaImage));
				scratchImage = std::move(premultipliedAlphaImage);
				metadata = scratchImage.GetMetadata();
			}

			if (flags & ETextureLoadFlagsCompress) {
				DirectX::ScratchImage compressedImage;
				DirectX::TEX_COMPRESS_FLAGS compressFlags = DirectX::TEX_COMPRESS_PARALLEL;
				DXGI_FORMAT compressedFormat = DirectX::IsSRGB(metadata.format) ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;

				ThrowIfFailed(DirectX::Compress(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata,
					compressedFormat, compressFlags, DirectX::TEX_THRESHOLD_DEFAULT, compressedImage));
				scratchImage = std::move(compressedImage);
				metadata = scratchImage.GetMetadata();
			}

			if (flags & ETextureLoadFlagsCacheToDds) {
				fs::create_directories(cacheFilepath.parent_path());
				ThrowIfFailed(DirectX::SaveToDDSFile(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::DDS_FLAGS_NONE, cacheFilepath.c_str()));
			}
		}

		if (flags & ETextureLoadFlagsAllocateFullMipChain) {
			metadata.mipLevels = 0;
		}

		switch (metadata.dimension) {
			case DirectX::TEX_DIMENSION_TEXTURE1D:
				textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, metadata.width, (uint16)metadata.arraySize, (uint16)metadata.mipLevels);
				break;
			case DirectX::TEX_DIMENSION_TEXTURE2D:
				textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width, (uint32)metadata.height, (uint16)metadata.arraySize, (uint16)metadata.mipLevels);
				break;
			case DirectX::TEX_DIMENSION_TEXTURE3D:
				textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(metadata.format, metadata.width, (uint32)metadata.height, (uint16)metadata.depth, (uint16)metadata.mipLevels);
				break;
			default:
				assert(false);
				break;
		}

		return true;
	}

	DxTexture LoadTextureInternal(const char* filename, uint32 flags) {
		DirectX::ScratchImage scratchImage;
		D3D12_RESOURCE_DESC textureDesc;

		LoadImageFromFile(filename, flags, scratchImage, textureDesc);

		const DirectX::Image* images = scratchImage.GetImages();
		uint32 numImages = (uint32)scratchImage.GetImageCount();

		D3D12_SUBRESOURCE_DATA subresources[64];
		for (uint32 i = 0; i < numImages; i++) {
			D3D12_SUBRESOURCE_DATA subresource = subresources[i];
			subresource.RowPitch = images[i].rowPitch;
			subresource.pData = images[i].pixels;
		}

		DxTexture result = DxTexture::Create(textureDesc, subresources, numImages);
		return result;
	}
}

void DxTexture::UploadSubresourceData(D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources) {
	DxContext& dxContext = DxContext::Instance();
	DxCommandList* commandList = dxContext.GetFreeCopyCommandList();
	commandList->TransitionBarrier(Resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	uint64 requiredSize = GetRequiredIntermediateSize(Resource.Get(), firstSubresource, numSubresources);

	DxResource intermediateResource;
	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
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

void DxTexture::Update(const char *filename, uint32 flags) {
	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC textureDesc;

	LoadImageFromFile(filename, flags, scratchImage, textureDesc);

	const DirectX::Image* images = scratchImage.GetImages();
	uint32 numImages = (uint32)scratchImage.GetImageCount();

	D3D12_SUBRESOURCE_DATA subResources[64];
	for (uint32 i = 0; i < numImages; i++) {
		D3D12_SUBRESOURCE_DATA& subresource = subResources[i];
		subresource.RowPitch = images[i].rowPitch;
		subresource.SlicePitch = images[i].slicePitch;
		subresource.pData = images[i].pixels;
	}

	UploadSubresourceData(subResources, 0, numImages);
}

DxTexture DxTexture::LoadFromFile(const char *filename, uint32 flags) {
	DxTexture result = LoadTextureInternal(filename, flags);
	// Todo: cache
	return result;
}

DxTexture DxTexture::Create(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState) {
	DxContext& dxContext = DxContext::Instance();
	DxTexture result;

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(result.Resource.GetAddressOf())
	));

	result.FormatSupport.Format = textureDesc.Format;
	ThrowIfFailed(dxContext.GetDevice()->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&result.FormatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)
	));

	if ((textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 and result.FormatSupportsRTV()) {
		result.RTVHandles = dxContext.PushRenderTargetView(&result);
	}

	if ((textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 9 and (result.FormatSupportsDSV() or IsDepthFormat(textureDesc.Format))) {
		result.DSVHandle = dxContext.PushDepthStencilView(&result);
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
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(result.Resource.GetAddressOf())
	));

	result.FormatSupport.Format = format;
	ThrowIfFailed(dxContext.GetDevice()->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&result.FormatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)
	));

	result.DSVHandle = dxContext.PushDepthStencilView(&result);

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
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		state,
		clearValue,
		IID_PPV_ARGS(Resource.GetAddressOf())
	));

	if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 and FormatSupportsRTV()) {

		dxContext.CreateRenderTargetView(this, RTVHandles);
	}

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
		dxContext.CreateDepthStencilView(this, DSVHandle);
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

