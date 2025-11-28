#include "pbr.hpp"
#include "../directx/DxTexture.h"
#include "TexturePreprocessing.h"
#include "../directx/DxContext.h"
#include "../directx/DxCommandList.h"
#include "../directx/DxRenderer.h"
#include "../physics/geometry.h"

#include "default_pbr_rs.hlsli"
#include "material.hlsli"

#include <unordered_map>

struct MaterialKey {
	std::string AlbedoTex, NormalTex, RoughTex, MetallicTex;
	vec4 Emission;
	vec4 AlbedoTint;
	float RoughnessOverride, MetallicOverride;
};

namespace std {
	// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
	template <typename T>
	inline void hash_combine(std::size_t& seed, const T& v) {
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
}

namespace std {
	template<>
	struct hash<vec4> {
		size_t operator()(const vec4& x) const {
			std::size_t seed = 0;

			hash_combine(seed, x.x);
			hash_combine(seed, x.y);
			hash_combine(seed, x.z);
			hash_combine(seed, x.w);

			return seed;
		}
	};

	template<>
	struct hash<MaterialKey> {
		size_t operator()(const MaterialKey& x) const {
			std::size_t seed = 0;

			hash_combine(seed, x.AlbedoTex);
			hash_combine(seed, x.NormalTex);
			hash_combine(seed, x.RoughTex);
			hash_combine(seed, x.MetallicTex);
			hash_combine(seed, x.Emission);
			hash_combine(seed, x.AlbedoTint);
			hash_combine(seed, x.RoughnessOverride);
			hash_combine(seed, x.MetallicOverride);

			return seed;
		}
	};
}

static bool operator==(const MaterialKey& a, const MaterialKey& b) {
	return a.AlbedoTex == b.AlbedoTex
		&& a.NormalTex == b.NormalTex
		&& a.RoughTex == b.RoughTex
		&& a.MetallicTex == b.MetallicTex
		&& a.Emission == b.Emission
		&& a.AlbedoTint == b.AlbedoTint
		&& a.RoughnessOverride == b.RoughnessOverride
		&& a.MetallicOverride == b.MetallicOverride;
}

static DxPipeline defaultOpaquePBRPipeline;
static DxPipeline defaultTransparentPBRPipeline;
static std::unordered_map<MaterialKey, WeakPtr<PbrMaterial>> materialCache;
static std::mutex materialMutex;
static std::unordered_map<std::string, WeakPtr<PbrEnvironment>> environmentCache;
static std::mutex environmentMutex;

Ptr<PbrMaterial> CreatePBRMaterial(const char *albedoTex, const char *normalTex, const char *roughTex, const char *metallicTex, const vec4 &emission, const vec4 &albedoTint, float roughOverride, float metallicOverride) {
	MaterialKey s = {
		albedoTex ? albedoTex : "",
		normalTex ? normalTex : "",
		roughTex ? roughTex : "",
		metallicTex ? metallicTex : "",
		emission,
		albedoTint,
		roughTex ? 1.f : roughOverride, // If texture is set, override does not matter, so set it to consistent value.
		metallicTex ? 0.f : metallicOverride, // If texture is set, override does not matter, so set it to consistent value.
	};

	materialMutex.lock();

	auto sp = materialCache[s].lock();
	TextureFactory* textureFactory = TextureFactory::Instance();
	if (!sp) {
		Ptr<PbrMaterial> material = MakePtr<PbrMaterial>();

		if (albedoTex) material->Albedo = textureFactory->LoadTextureFromFile(albedoTex);
		if (normalTex) material->Normal = textureFactory->LoadTextureFromFile(normalTex, ETextureLoadFlagsDefault | ETextureLoadFlagsNoncolor);
		if (roughTex) material->Roughness = textureFactory->LoadTextureFromFile(roughTex, ETextureLoadFlagsDefault | ETextureLoadFlagsNoncolor);
		if (metallicTex) material->Metallic = textureFactory->LoadTextureFromFile(metallicTex, ETextureLoadFlagsDefault | ETextureLoadFlagsNoncolor);
		material->Emission = emission;
		material->AlbedoTint = albedoTint;
		material->RoughnessOverride = roughOverride;
		material->MetallicOverride = metallicOverride;

		materialCache[s] = sp = material;
	}

	materialMutex.unlock();
	return sp;
}

Ptr<PbrMaterial> GetDefaultPBRMaterial() {
	static Ptr<PbrMaterial> material = MakePtr<PbrMaterial>(nullptr, nullptr, nullptr, nullptr, vec4(0.f), vec4(1.f, 0.f, 1.f, 1.f), 1.f, 0.f);
	return material;
}

Ptr<PbrEnvironment> CreateEnvironment(const char *filename, uint32 skyResolution, uint32 environmentResolution, uint32 irradianceResolution, bool asyncCompute) {
	environmentMutex.lock();

	std::string s = filename;

	auto sp = environmentCache[s].lock();
	DxContext& dxContext = DxContext::Instance();
	TextureFactory* textureFactory = TextureFactory::Instance();
	TexturePreprocessor* texturePreprocessor = TexturePreprocessor::Instance();
	if (!sp) {
		Ptr<DxTexture> equiSky = textureFactory->LoadTextureFromFile(filename,
			ETextureLoadFlagsNoncolor | ETextureLoadFlagsCacheToDds | ETextureLoadFlagsGenMipsOnCpu);

		if (equiSky) {
			Ptr<PbrEnvironment> environment = MakePtr<PbrEnvironment>();

			DxCommandList* cl;
			if (asyncCompute) {
				dxContext.ComputeQueue.WaitForOtherQueue(dxContext.CopyQueue);
				cl = dxContext.GetFreeComputeCommandList(true);
			}
			else {
				dxContext.RenderQueue.WaitForOtherQueue(dxContext.CopyQueue);
				cl = dxContext.GetFreeRenderCommandList();
			}
			//generateMipMapsOnGPU(cl, equiSky);
			environment->Sky = texturePreprocessor->EquirectangularToCubemap(cl, equiSky, skyResolution, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
			environment->Environment = texturePreprocessor->PrefilterEnvironment(cl, environment->Sky, environmentResolution);
			environment->Irradiance = texturePreprocessor->CubemapToIrradiance(cl, environment->Sky, irradianceResolution);

			SET_NAME(environment->Sky->Resource(), "Sky");
			SET_NAME(environment->Environment->Resource(), "Environment");
			SET_NAME(environment->Irradiance->Resource(), "Irradiance");

			dxContext.ExecuteCommandList(cl);

			environmentCache[s] = sp = environment;
		}
	}

	environmentMutex.unlock();
	return sp;
}

void PbrMaterial::PrepareForRendering(DxCommandList* cl) {
	uint32 flags = 0;

	if (Albedo) {
		cl->SetDescriptorHeapSRV(DefaultPbrRsPbrTextures, 0, Albedo);
		flags |= USE_ALBEDO_TEXTURE;
	}
	if (Normal) {
		cl->SetDescriptorHeapSRV(DefaultPbrRsPbrTextures, 1, Normal);
		flags |= USE_NORMAL_TEXTURE;
	}
	if (Roughness) {
		cl->SetDescriptorHeapSRV(DefaultPbrRsPbrTextures, 2, Roughness);
		flags |= USE_ROUGHNESS_TEXTURE;
	}
	if (Metallic) {
		cl->SetDescriptorHeapSRV(DefaultPbrRsPbrTextures, 3, Metallic);
		flags |= USE_METALLIC_TEXTURE;
	}

	cl->SetGraphics32BitConstants(DefaultPbrRsMaterial,
		PbrMaterialCb(AlbedoTint, Emission.xyz, RoughnessOverride, MetallicOverride, flags)
	);
}

void PbrMaterial::SetupOpaquePipeline(DxCommandList* cl, const CommonMaterialInfo& info) {
	cl->SetPipelineState(*defaultOpaquePBRPipeline.Pipeline);
	cl->SetGraphicsRootSignature(*defaultOpaquePBRPipeline.RootSignature);

	SetupCommon(cl, info);
}

void PbrMaterial::SetupTransparentPipeline(DxCommandList* cl, const CommonMaterialInfo& info) {
	cl->SetPipelineState(*defaultTransparentPBRPipeline.Pipeline);
	cl->SetGraphicsRootSignature(*defaultTransparentPBRPipeline.RootSignature);

	SetupCommon(cl, info);
}

void PbrMaterial::SetupCommon(DxCommandList* cl, const CommonMaterialInfo& info) {
	DxCpuDescriptorHandle nullTexture = DxRenderer::Instance()->NullTextureSRV;
	DxCpuDescriptorHandle nullBuffer = DxRenderer::Instance()->NullBufferSRV;

	cl->SetGraphics32BitConstants(DefaultPbrRsLighting, LightingCb{ vec2(1.f / info.ShadowMap->Width(), 1.f / info.ShadowMap->Height()), info.EnvironmentIntensity });

	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 0, info.Irradiance);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 1, info.Environment);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 2, info.Brdf);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 3, info.TiledCullingGrid);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 4, info.TiledObjectsIndexList);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 5, info.PointLightBuffer ? info.PointLightBuffer->DefaultSRV : nullBuffer);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 6, info.SpotlightBuffer ? info.SpotlightBuffer->DefaultSRV : nullBuffer);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 7, info.DecalBuffer ? info.DecalBuffer->DefaultSRV : nullBuffer);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 8, info.ShadowMap ? info.ShadowMap->DefaultSRV() : nullTexture);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 9, info.PointLightShadowInfoBuffer ? info.PointLightShadowInfoBuffer->DefaultSRV : nullBuffer);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 10, info.SpotlightShadowInfoBuffer ? info.SpotlightShadowInfoBuffer->DefaultSRV : nullBuffer);
	cl->SetDescriptorHeapSRV(DefaultPbrRsFrameConstants, 11, info.DecalTextureAtlas ? info.DecalTextureAtlas->DefaultSRV() : nullTexture);

	cl->SetGraphicsDynamicConstantBuffer(DefaultPbrRsSub, info.SunCBV);

	cl->SetGraphicsDynamicConstantBuffer(DefaultPbrRsCamera, info.CameraCBV);


	// Default material properties. This is JUST to make the dynamic descriptor heap happy.
	// These textures will NEVER be read.

	cl->SetDescriptorHeapSRV(DefaultPbrRsPbrTextures, 0, nullTexture);
	cl->SetDescriptorHeapSRV(DefaultPbrRsPbrTextures, 1, nullTexture);
	cl->SetDescriptorHeapSRV(DefaultPbrRsPbrTextures, 2, nullTexture);
	cl->SetDescriptorHeapSRV(DefaultPbrRsPbrTextures, 3, nullTexture);
}

void PbrMaterial::InitializePipeline() {
	DxPipelineFactory* pipelineFactory = DxPipelineFactory::Instance();
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.InputLayout(inputLayoutPositionUvNormalTangent)
			.RenderTargets(DxRenderer::opaqueLightPassFormats, arraysize(DxRenderer::opaqueLightPassFormats), DxRenderer::hdrDepthStencilFormat)
			.DepthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL);

		defaultOpaquePBRPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "default_vs", "default_pbr_ps" });
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.InputLayout(inputLayoutPositionUvNormalTangent)
			.RenderTargets(DxRenderer::transparentLightPassFormats, arraysize(DxRenderer::transparentLightPassFormats), DxRenderer::hdrDepthStencilFormat)
			.AlphaBlending(0);

		defaultTransparentPBRPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "default_vs", "default_pbr_transparent_ps" });
	}
}
