#include "DxTexture.h"
#include <filesystem>
#include <iostream>
#include "DxBarrierBatcher.h"
#include "DxContext.h"
#include "DirectXTex.h"
#include "../render/TexturePreprocessing.h"
#include "DxCommandList.h"

namespace fs = std::filesystem;

TextureFactory* TextureFactory::_instance = new TextureFactory{};

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

	bool LoadImageFromFile(fs::path filepath, uint32 flags, DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc) {
		if (flags & ETextureLoadFlagsGenMipsOnGpu) {
			flags &= ~ETextureLoadFlagsGenMipsOnCpu;
			flags |= ETextureLoadFlagsAllocateFullMipChain;
		}

		fs::path extension = filepath.extension();

		fs::path cachedFilename = filepath;
		cachedFilename.replace_extension("." + std::to_string(flags) + ".cache.dds");

		fs::path cacheFilepath = L"asset_cache" / cachedFilename;

		bool fromCache = false;
		DirectX::TexMetadata metadata;

		if (!(flags & ETextureLoadFlagsAlwaysLoadFromSource)) {
			// Look for cached.

			WIN32_FILE_ATTRIBUTE_DATA cachedData;
			if (GetFileAttributesExW(cacheFilepath.c_str(), GetFileExInfoStandard, &cachedData)) {
				FILETIME cachedFiletime = cachedData.ftLastWriteTime;

				WIN32_FILE_ATTRIBUTE_DATA originalData;
				assert(GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &originalData));
				FILETIME originalFiletime = originalData.ftLastWriteTime;

				if (CompareFileTime(&cachedFiletime, &originalFiletime) >= 0) {
					// Cached file is newer than original, so load this.
					fromCache = SUCCEEDED(DirectX::LoadFromDDSFile(cacheFilepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
				}
			}
		}

		if (not fromCache) {
			if (not fs::exists(filepath)) {
				std::cerr << "Couldn't find file '" << filepath.string() << "'." << std::endl;
				return false;
			}

			if (flags & ETextureLoadFlagsCacheToDds) {
				std::cout << "Preprocessing asset '" << filepath.string() << "' for faster loading next time.";
#ifdef _DEBUG
				std::cout << " Consider running in a release build the first time.";
#endif
				std::cout << std::endl;
			}

			if (extension == ".dds") {
				if (FAILED(DirectX::LoadFromDDSFile(filepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage))) {
					return false;
				}
			}
			else if (extension == ".hdr") {
				if (FAILED(DirectX::LoadFromHDRFile(filepath.c_str(), &metadata, scratchImage))) {
					return false;
				}
			}
			else if (extension == ".tga") {
				if (FAILED(DirectX::LoadFromTGAFile(filepath.c_str(), &metadata, scratchImage))) {
					return false;
				}
			}
			else {
				if (FAILED(DirectX::LoadFromWICFile(filepath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage))) {
					return false;
				}
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
				if (metadata.width % 4 == 0 and metadata.height % 4 == 0) {
					if (not DirectX::IsCompressed(metadata.format)) {
						uint32 numChannels = DxTexture::GetNumberOfChannels(metadata.format);

						DXGI_FORMAT compressedFormat;

						switch (numChannels) {
							case 1: compressedFormat = DXGI_FORMAT_BC4_UNORM; break;
							case 2: compressedFormat = DXGI_FORMAT_BC5_UNORM; break;
							case 3:
							case 4: {
								if (scratchImage.IsAlphaAllOpaque()) {
									compressedFormat = DirectX::IsSRGB(metadata.format) ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
								}
								else {
									compressedFormat = DirectX::IsSRGB(metadata.format) ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM; // 7 would be better, but takes forever to compress.
								}
								break;
							}
						}

						DirectX::ScratchImage compressedImage;
						DirectX::TEX_COMPRESS_FLAGS compressFlags = DirectX::TEX_COMPRESS_PARALLEL;
						ThrowIfFailed(DirectX::Compress(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata,
						compressedFormat, compressFlags, DirectX::TEX_THRESHOLD_DEFAULT, compressedImage));
						metadata = scratchImage.GetMetadata();
					}
				}
				else {
					std::cerr << "Cannot compress texture '" << filepath << "', since its dimensions are not a multiple of 4." << std::endl;
				}
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

	bool CheckFormatSupport(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport, D3D12_FORMAT_SUPPORT1 support) {
		return (formatSupport.Support1 & support) != 0;
	}

	bool CheckFormatSupport(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport, D3D12_FORMAT_SUPPORT2 support) {
		return (formatSupport.Support2 & support) != 0;
	}

	bool FormatSupportsRTV(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport) {
		return CheckFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
	}

	bool FormatSupportsDSV(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport) {
		return CheckFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
	}

	bool FormatSupportsSRV(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport) {
		return CheckFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
	}

	bool FormatSupportsUAV(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport) {
		return CheckFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
			CheckFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
			CheckFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
	}

	void Retire(DxResource resource, DxCpuDescriptorHandle srv, DxCpuDescriptorHandle uav, DxCpuDescriptorHandle stencil, DxRtvDescriptorHandle rtv, DxDsvDescriptorHandle dsv, std::vector<DxCpuDescriptorHandle>&& mipUAVs) {
		TextureGrave grave;
		grave.Resource = resource;
		grave.Srv = srv;
		grave.Uav = uav;
		grave.Stencil = stencil;
		grave.Rtv = rtv;
		grave.Dsv = dsv;
		grave.MipUAVs = std::move(mipUAVs);
		DxContext::Instance().Retire(std::move(grave));
	}
}

bool DxTexture::IsUAVCompatibleFormat(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SINT:
			return true;
		default:
			return false;
	}
}

bool DxTexture::IsSRGBFormat(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return true;
		default:
			return false;
	}
}

bool DxTexture::IsBGRFormat(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return true;
		default:
			return false;
	}
}

bool DxTexture::IsDepthFormat(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_D16_UNORM:
			return true;
		default:
			return false;
	}
}

bool DxTexture::IsStencilFormat(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			return true;
		default:
			return false;
	}
}

DXGI_FORMAT DxTexture::GetSRGBFormat(DXGI_FORMAT format) {
	DXGI_FORMAT srgbFormat = format;
	switch (format) {
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			srgbFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
		case DXGI_FORMAT_BC1_UNORM:
			srgbFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
			break;
		case DXGI_FORMAT_BC2_UNORM:
			srgbFormat = DXGI_FORMAT_BC2_UNORM_SRGB;
			break;
		case DXGI_FORMAT_BC3_UNORM:
			srgbFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			srgbFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			break;
		case DXGI_FORMAT_B8G8R8X8_UNORM:
			srgbFormat = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
			break;
		case DXGI_FORMAT_BC7_UNORM:
			srgbFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
			break;
	}

	return srgbFormat;
}

DXGI_FORMAT DxTexture::GetUAVCompatibleFormat(DXGI_FORMAT format) {
	DXGI_FORMAT uavFormat = format;

	switch (format) {
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
			uavFormat = DXGI_FORMAT_R32_FLOAT;
			break;
	}

	return uavFormat;
}

DXGI_FORMAT DxTexture::GetDepthReadFormat(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_D16_UNORM: return DXGI_FORMAT_R16_UNORM;
	}

	return format;
}

DXGI_FORMAT DxTexture::GetStencilReadFormat(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
	}

	return format;
}

uint32 DxTexture::GetFormatSize(DXGI_FORMAT format) {
	uint32 size = 0;

	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			size = 4 * 4;
			break;
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
		case DXGI_FORMAT_R32G32B32_TYPELESS:
			size = 3 * 4;
			break;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			size = 4 * 2;
			break;
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			size = 2 * 4;
			break;
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R11G11B10_FLOAT:
			size = 4;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			size = 4;
			break;
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R16G16_TYPELESS:
			size = 2 * 2;
			break;
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R32_TYPELESS:
			size = 4;
			break;
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R8G8_TYPELESS:
			size = 2;
			break;
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
			size = 2;
			break;
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_R8_TYPELESS:
			size = 1;
			break;
			size = 4;
			break;

		default:
			assert(false); // Compressed format.
	}

	return size;
}

DXGI_FORMAT DxTexture::GetTypelessFormat(DXGI_FORMAT format) {
	DXGI_FORMAT typelessFormat = format;

	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
			typelessFormat = DXGI_FORMAT_R32G32B32A32_TYPELESS;
			break;
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			typelessFormat = DXGI_FORMAT_R32G32B32_TYPELESS;
			break;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
			typelessFormat = DXGI_FORMAT_R16G16B16A16_TYPELESS;
			break;
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
			typelessFormat = DXGI_FORMAT_R32G32_TYPELESS;
			break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			typelessFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
			break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			typelessFormat = DXGI_FORMAT_R24G8_TYPELESS;
			break;
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
			typelessFormat = DXGI_FORMAT_R10G10B10A2_TYPELESS;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
			typelessFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
			break;
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
			typelessFormat = DXGI_FORMAT_R16G16_TYPELESS;
			break;
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
			typelessFormat = DXGI_FORMAT_R32_TYPELESS;
			break;
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
			typelessFormat = DXGI_FORMAT_R8G8_TYPELESS;
			break;
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
			typelessFormat = DXGI_FORMAT_R16_TYPELESS;
			break;
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
			typelessFormat = DXGI_FORMAT_R8_TYPELESS;
			break;
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			typelessFormat = DXGI_FORMAT_BC1_TYPELESS;
			break;
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			typelessFormat = DXGI_FORMAT_BC2_TYPELESS;
			break;
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			typelessFormat = DXGI_FORMAT_BC3_TYPELESS;
			break;
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			typelessFormat = DXGI_FORMAT_BC4_TYPELESS;
			break;
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			typelessFormat = DXGI_FORMAT_BC5_TYPELESS;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			typelessFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
			break;
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			typelessFormat = DXGI_FORMAT_B8G8R8X8_TYPELESS;
			break;
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			typelessFormat = DXGI_FORMAT_BC6H_TYPELESS;
			break;
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			typelessFormat = DXGI_FORMAT_BC7_TYPELESS;
			break;
	}

	return typelessFormat;
}

uint32 DxTexture::GetNumberOfChannels(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_A8_UNORM:
		case DXGI_FORMAT_R1_UNORM:
			return 1;
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
			return 2;
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
			return 3;
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
			return 4;
		default:
			assert(false);
			return 0;
	}
}

Ptr<DxTexture> TextureFactory::LoadVolumeTextureInternal(const std::string& dirname, uint32 flags) {
	// No mip maps allowed for now!
	assert(not (flags & ETextureLoadFlagsAllocateFullMipChain));
	assert(not (flags & ETextureLoadFlagsGenMipsOnCpu));
	assert(not (flags & ETextureLoadFlagsGenMipsOnGpu));

	std::vector<DirectX::ScratchImage> scratchImages;
	D3D12_RESOURCE_DESC textureDesc = {};

	uint32 totalSize = 0;

	for (auto& p : fs::directory_iterator(dirname)) {
		auto& path = p.path();
		DirectX::ScratchImage& s = scratchImages.emplace_back();
		if (not LoadImageFromFile(p, flags, s, textureDesc)) {
			return nullptr;
		}

		assert(s.GetImageCount() == 1);
		const auto& image = s.GetImages()[0];

		if (scratchImages.size() > 1) {
			assert(image.width == scratchImages.begin()->GetImages()[0].width);
			assert(image.height == scratchImages.begin()->GetImages()[0].height);
			assert(image.slicePitch == scratchImages.begin()->GetImages()[0].slicePitch);
		}

		totalSize += (uint32)image.slicePitch;
	}

	uint32 width = (uint32)textureDesc.Width;
	uint32 height = textureDesc.Height;
	uint32 depth = (uint32)scratchImages.size();

	uint8* allPixels = new uint8[totalSize];

	for (uint32 i = 0; i < depth; i++) {
		DirectX::ScratchImage& s = scratchImages[i];
		const auto& image = s.GetImages()[0];

		memcpy(allPixels + i * image.slicePitch, image.pixels, image.slicePitch);
	}

	D3D12_SUBRESOURCE_DATA subresource;
	subresource.RowPitch = scratchImages.begin()->GetImages()[0].rowPitch;
	subresource.SlicePitch = scratchImages.begin()->GetImages()[0].slicePitch;
	subresource.pData = allPixels;

	Ptr<DxTexture> result = TextureFactory::Instance()->CreateVolumeTexture(0, width, height, depth, textureDesc.Format, false);
	result->UploadSubresourceData(&subresource, 0, 1);

	delete[] allPixels;

	return result;
}

Ptr<DxTexture> TextureFactory::LoadTextureInternal(const std::string& filename, uint32 flags) {
	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC textureDesc;

	if (not LoadImageFromFile(filename, flags, scratchImage, textureDesc)) {
		return nullptr;
	}

	const DirectX::Image* images = scratchImage.GetImages();
	auto numImages = (uint32)scratchImage.GetImageCount();

	std::unique_ptr<D3D12_SUBRESOURCE_DATA[]> subresourceData(new(std::nothrow)D3D12_SUBRESOURCE_DATA[numImages]);
	for (uint32 i = 0; i < numImages; i++) {
		D3D12_SUBRESOURCE_DATA& subresource = subresourceData[i];
		subresource.RowPitch = images[i].rowPitch;
		subresource.SlicePitch = images[i].slicePitch;
		subresource.pData = images[i].pixels;
	}

	Ptr<DxTexture> result = TextureFactory::Instance()->CreateTexture(textureDesc, subresourceData.get(), numImages);
	SET_NAME(result->_resource, "Loaded from file");

	if (flags & ETextureLoadFlagsGenMipsOnGpu) {
		DxContext& context = DxContext::Instance();
		context.RenderQueue.WaitForOtherQueue(context.CopyQueue);
		DxCommandList* list = context.GetFreeRenderCommandList();
		TexturePreprocessor* preprocessor = TexturePreprocessor::Instance();
		preprocessor->GenerateMipMapsOnGPU(list, result);
		context.ExecuteCommandList(list);
	}

	return result;
}

DxTexture::DxTexture(DxResource resource, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle) {
	_resource = resource;
	_rtvHandles = rtvHandle;
}


Ptr<DxTexture> TextureFactory::LoadTextureFromFile(const char *filename, uint32 flags) {
	_mutex.lock();

	std::string s = filename;

	auto sp = _textureCache[s].lock();
	if (not sp) {
		_textureCache[s] = sp = LoadTextureInternal(s, flags);
	}

	_mutex.unlock();
	return sp;
}

Ptr<DxTexture> TextureFactory::LoadVolumeTextureFromDirectory(const char *dirname, uint32 flags) {
	_mutex.lock();

	std::string s = dirname;

	auto sp = _textureCache[s].lock();
	if (!sp) {
		_textureCache[s] = sp = LoadVolumeTextureInternal(s, flags);
	}

	_mutex.unlock();
	return sp;
}

void DxTexture::UploadSubresourceData(D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresource) {
	DxContext& dxContext = DxContext::Instance();
	DxCommandList* commandList = dxContext.GetFreeCopyCommandList();
	commandList->TransitionBarrier(_resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	uint64 requiredSize = GetRequiredIntermediateSize(_resource.Get(), firstSubresource, numSubresource);

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

	UpdateSubresources(commandList->CommandList(), _resource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresource, subresourceData);
	dxContext.Retire(intermediateResource);

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

Ptr<DxTexture> TextureFactory::CreateTexture(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA *subresourceData,
                                 uint32 numSubresources, D3D12_RESOURCE_STATES initialState) {
	DxContext& dxContext = DxContext::Instance();

	Ptr<DxTexture> result = MakePtr<DxTexture>();

	result->_requestedNumMipLevels = textureDesc.MipLevels;

	uint32 maxNumMipLevels = (uint32)log2(max(textureDesc.Width, textureDesc.Height)) + 1;
	textureDesc.MipLevels = min(maxNumMipLevels, result->_requestedNumMipLevels);

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;
	formatSupport.Format = textureDesc.Format;
	ThrowIfFailed(dxContext.GetDevice()->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)
	));

	result->_supportsRTV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) and FormatSupportsRTV(formatSupport);
	result->_supportsDSV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) and FormatSupportsDSV(formatSupport);
	result->_supportsUAV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) and FormatSupportsUAV(formatSupport);
	result->_supportsSRV = FormatSupportsSRV(formatSupport);

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET and !result->_supportsRTV) {
		std::cout << "Warning. Requested RTV, but not supported by format." << std::endl;
		__debugbreak();
	}

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL and !result->_supportsDSV) {
		std::cout << "Warning. Requested DSV, but not supported by format." << std::endl;
		__debugbreak();
	}

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS and !result->_supportsUAV) {
		std::cout << "Warning. Requested UAV, but not supported by format." << std::endl;
		__debugbreak();
	}

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&result->_resource)
	));

	result->_numMipLevels = result->_resource->GetDesc().MipLevels;
	result->_width = (uint32)textureDesc.Width;
	result->_height = textureDesc.Height;
	result->_depth = textureDesc.DepthOrArraySize;

	result->_defaultSRV = {};
	result->_defaultUAV = {};
	result->_rtvHandles = {};
	result->_dsvHandle = {};
	result->_stencilSRV = {};

	result->_initialState = initialState;

	// Upload
	if (subresourceData) {
		result->UploadSubresourceData(subresourceData, 0, numSubresources);
	}

	// SRV
	if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
		result->_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateVolumeTextureSRV(result.get());
	}
	else if (textureDesc.DepthOrArraySize == 6) {
		result->_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateCubemapSRV(result.get());
	}
	else {
		result->_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().Create2DTextureSRV(result.get());
	}

	// RTV
	if (result->_supportsRTV) {
		result->_rtvHandles = dxContext.RtvAllocator().GetFreeHandle().Create2DTextureRTV(result.get());
	}

	// UAV
	if (result->_supportsUAV) {
		if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
			result->_defaultUAV = dxContext.DescriptorAllocatorGPU().GetFreeHandle().CreateVolumeTextureUAV(result.get());
		}
		else if (textureDesc.DepthOrArraySize == 6) {
			result->_defaultUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateCubemapUAV(result.get());
		}
		else {
			result->_defaultUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().Create2DTextureUAV(result.get());
		}
	}

	return result;
}

Ptr<DxTexture> TextureFactory::CreateTexture(const void *data, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips,
                                 bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState) {
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE)
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE);

	uint32 numMips = allocateMips ? 0 : 1;
	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, numMips, 1, 0, flags);

	if (data) {
		uint32 formatSize = DxTexture::GetFormatSize(textureDesc.Format);

		D3D12_SUBRESOURCE_DATA subresource;
		subresource.RowPitch = width * formatSize;
		subresource.SlicePitch = width * height * formatSize;
		subresource.pData = data;

		return CreateTexture(textureDesc, &subresource, 1, initialState);
	}
	return CreateTexture(textureDesc, nullptr, 0, initialState);
}

Ptr<DxTexture> TextureFactory::CreateDepthTexture(uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength,
                                      D3D12_RESOURCE_STATES initialState) {
	DxContext& dxContext = DxContext::Instance();
	Ptr<DxTexture> result = MakePtr<DxTexture>();

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.DepthStencil = { 1.f, 0 };
	DXGI_FORMAT typelessFormat = DxTexture::GetTypelessFormat(format);

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(typelessFormat, width, height,
		arrayLength, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(result->_resource.GetAddressOf())
	));

	result->_numMipLevels = 1;
	result->_requestedNumMipLevels = 1;
	result->_format = format;
	result->_width = width;
	result->_height = height;
	result->_depth = arrayLength;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;
	formatSupport.Format = format;
	ThrowIfFailed(dxContext.GetDevice()->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)
	));

	result->_supportsRTV = false;
	result->_supportsDSV = FormatSupportsDSV(formatSupport);
	result->_supportsUAV = false;
	result->_supportsSRV = FormatSupportsSRV(formatSupport);

	result->_defaultSRV = {};
	result->_defaultUAV = {};
	result->_rtvHandles = {};
	result->_dsvHandle = {};
	result->_stencilSRV = {};

	result->_initialState = initialState;

	assert(result->_supportsDSV);

	result->_dsvHandle = dxContext.DsvAllocator().GetFreeHandle().Create2DTextureDSV(result.get());
	if (arrayLength == 1) {
		result->_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateDepthTextureSRV(result.get());
		if (DxTexture::IsStencilFormat(format)) {
			result->_stencilSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateStencilTextureSRV(result.get());
		}
	}
	else {
		result->_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateDepthTextureArraySRV(result.get());
	}

	return result;
}

Ptr<DxTexture> TextureFactory::CreateCubeTexture(const void *data, uint32 width, uint32 height, uint32 depth, DXGI_FORMAT format, bool allocateMips, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState) {
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE)
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE);

	uint32 numMips = allocateMips ? 0 : 1;
	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 6, numMips, 1, 0, flags);

	if (data) {
		uint32 formatSize = DxTexture::GetFormatSize(textureDesc.Format);

		D3D12_SUBRESOURCE_DATA subresources[6];
		for (uint32 i = 0; i < 6; i++) {
			auto& subresource = subresources[i];
			subresource.RowPitch = width * formatSize;
			subresource.SlicePitch = width * height * formatSize;
			subresource.pData = data;
		}

		return CreateTexture(textureDesc, subresources, 6, initialState);
	}
	return CreateTexture(textureDesc, 0, 0, initialState);
}

Ptr<DxTexture> TextureFactory::CreateVolumeTexture(const void *data, uint32 width, uint32 height, uint32 depth, DXGI_FORMAT format, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState) {
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE);

	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(format, width, height, depth, 1, flags);

	if (data) {
		uint32 formatSize = DxTexture::GetFormatSize(textureDesc.Format);

		D3D12_SUBRESOURCE_DATA* subresources = (D3D12_SUBRESOURCE_DATA*)alloca(sizeof(D3D12_SUBRESOURCE_DATA) * depth);
		for (uint32 i = 0; i < depth; i++) {
			auto& subresource = subresources[i];
			subresource.RowPitch = width * formatSize;
			subresource.SlicePitch = width * height * formatSize;
			subresource.pData = data;
		}

		return CreateTexture(textureDesc, subresources, depth, initialState);
	}
	return CreateTexture(textureDesc, nullptr, 0, initialState);
}

void DxTexture::SetName(const wchar *name) {
	ThrowIfFailed(_resource->SetName(name));
}

std::wstring DxTexture::GetName() const {
	if (not _resource) {
		return L"";
	}

	wchar name[128];
	uint32 size = sizeof(name);
	_resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
	name[min(arraysize(name) - 1, size)] = 0;

	return name;
}

void DxTexture::AllocateMipUAVs() {
	auto desc = _resource->GetDesc();
	assert(_supportsUAV);
	assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

	uint32 mipLevels = desc.MipLevels;
	for (uint32 i = 1; i < mipLevels; i++) {
		DxCpuDescriptorHandle handle = DxContext::Instance().DescriptorAllocatorCPU().GetFreeHandle().Create2DTextureUAV(this, i);
		_mipUAVs.push_back(handle);
	}
}

DxTexture::~DxTexture() {
	Retire(_resource, _defaultSRV, _defaultUAV, _stencilSRV, _rtvHandles, _dsvHandle, std::move(_mipUAVs));
}

void DxTexture::Resize(uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState) {
	DxContext& dxContext = DxContext::Instance();

	wchar name[128];
	uint32 size = sizeof(name);
	_resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
	name[min(arraysize(name) - 1, size)] = 0;

	bool hasMipUAVs = _mipUAVs.size() > 0;

	Retire(_resource, _defaultSRV, _defaultUAV, _stencilSRV, _rtvHandles, _dsvHandle, std::move(_mipUAVs));

	D3D12_RESOURCE_DESC desc = _resource->GetDesc();
	_resource.Reset();

	D3D12_RESOURCE_STATES state = initialState == -1 ? _initialState : initialState;
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	D3D12_CLEAR_VALUE* clearValue = nullptr;

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
		optimizedClearValue.Format = _format;
		optimizedClearValue.DepthStencil = { 1.f, 0 };
		clearValue = &optimizedClearValue;
	}

	uint32 maxNumMipLevels = (uint32)log2(max(newWidth, newHeight)) + 1;
	desc.MipLevels = min(maxNumMipLevels, _requestedNumMipLevels);

	desc.Width = newWidth;
	desc.Height = newHeight;
	_width = newWidth;
	_height = newHeight;

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(dxContext.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		state,
		clearValue,
		IID_PPV_ARGS(_resource.GetAddressOf())
	));

	_numMipLevels = _resource->GetDesc().MipLevels;

	if (_supportsRTV) {
		_rtvHandles = dxContext.RtvAllocator().GetFreeHandle().Create2DTextureRTV(this);
	}

	// DSV and SRV
	if (_supportsDSV) {
		_dsvHandle = dxContext.DsvAllocator().GetFreeHandle().Create2DTextureDSV(this);
		if (_depth == 1) {
			_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateDepthTextureSRV(this);
		}
		else {
			_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateDepthTextureArraySRV(this);
		}

		if (DxTexture::IsStencilFormat(_format)) {
			_stencilSRV = dxContext.DescriptorAllocatorGPU().GetFreeHandle().CreateStencilTextureSRV(this);
		}
	}
	else {
		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
			_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateVolumeTextureSRV(this);
		}
		else if (_depth == 6) {
			_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateCubemapSRV(this);
		}
		else {
			_defaultSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().Create2DTextureSRV(this);
		}
	}

	// UAV
	if (_supportsUAV) {
		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
			_defaultUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateVolumeTextureUAV(this);
		}
		else if (_depth == 6) {
			_defaultUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateCubemapUAV(this);
		}
		else {
			_defaultUAV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().Create2DTextureUAV(this);
		}
	}

	if (hasMipUAVs) {
		AllocateMipUAVs();
	}

	SetName(name);
}

TextureGrave::~TextureGrave() {
	wchar name[128];

	DxContext& dxContext = DxContext::Instance();
	if (Resource) {
		uint32 size = sizeof(name);
		Resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
		name[min(arraysize(name) - 1, size)] = 0;

		if (Srv.CpuHandle.ptr) {
			dxContext.DescriptorAllocatorCPU().FreeHandle(Srv);
		}
		if (Uav.CpuHandle.ptr) {
			dxContext.DescriptorAllocatorCPU().FreeHandle(Uav);
		}
		if (Stencil.CpuHandle.ptr) {
			dxContext.DescriptorAllocatorCPU().FreeHandle(Stencil);
		}
		if (Rtv.CpuHandle.ptr) {
			dxContext.RtvAllocator().FreeHandle(Rtv);
		}
		if (Dsv.CpuHandle.ptr) {
			dxContext.DsvAllocator().FreeHandle(Dsv);
		}

		for (DxCpuDescriptorHandle handle : MipUAVs) {
			dxContext.DescriptorAllocatorCPU().FreeHandle(handle);
		}
	}
}
