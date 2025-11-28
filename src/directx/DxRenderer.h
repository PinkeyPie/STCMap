#pragma once

#include "DxCommandList.h"
#include "../core/math.h"
#include "../core/camera.h"
#include "../render/RenderPass.h"
#include "../render/LightSource.h"
#include "../render/pbr.hpp"
#include "../core/input.h"
#include "../render/Raytracer.h"

#include "light_source.hlsli"
#include "camera.hlsli"
#include "volumetrics_rs.hlsli"
#include "post_processing_rs.hlsli"
#include "ssr_rs.hlsli"

#define SHADOW_MAP_WIDTH 6144
#define SHADOW_MAP_HEIGHT 6144

#define MAX_NUM_POINT_LIGHT_SHADOW_PASSES 16
#define MAX_NUM_SPOT_LIGHT_SHADOW_PASSES 16

enum AspectRatioMode {
	EAspectRatioFree,
	EAspectRatioFix16_9,
	EAspectRatioFix16_10,

	EAspectRatioModeCount
};

enum RendererMode {
	ERendererModeRasterized,
	ERendererModePathtraced,

	ERendererModeCount
};

struct RenderSettings {
	TonemapCb Tonemap = DefaultTonemapParameters();
	float EnvironmentIntensity = 1.f;
	float SkyIntensity = 1.f;

	AspectRatioMode AspectRatio = EAspectRatioFree;

	bool EnableSSR = true;
	SsrRaycastCb SSR = DefaultSSRParameters();

	bool EnableTemporalAntialiasing = true;
	float CameraJitterStrength = 1.f;

	bool EnableBloom = true;
	float BloomThreshold = 100.f;
	float BloomStrength = 0.1f;

	bool EnableSharpen = true;
	float SharpenStrength = 0.5f;
};

class DxRenderer {
public:
	DxRenderer() = default;

	static DxRenderer* Instance() {
		return _instance;
	}
	void Initialize(DXGI_FORMAT screenFormat, uint32 width, uint32 height, bool renderObjectIDs);

	void BeginFrameCommon();
	void EndFrameCommon();

	void BeginFrame(uint32 width, uint32 height);
	void BeginFrame(CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, DxResource renderTarget);
	void EndFrame(const UserInput& input);

	void SetCamera(const RenderCamera& camera);
	void SetEnvironment(const Ptr<PbrEnvironment>& environment);
	void SetSun(const DirectionalLight& light);

	void SetPointLights(const Ptr<DxBuffer>& lights, uint32 numLights);
	void SetSpotLights(const Ptr<DxBuffer>& lights, uint32 numLights);
	void SetDecals(const Ptr<DxBuffer>& decals, uint32 numLights, const Ptr<DxTexture>& textureAtlas);
	void SetWindowFrame();

	void SubmitRenderPass(OpaqueRenderPass* renderPass) {
		assert(!_opaqueRenderPass);
		_opaqueRenderPass = renderPass;
	}

	void SubmitRenderPass(TransparentRenderPass* renderPass) {
		assert(!_transparentRenderPass);
		_transparentRenderPass = renderPass;
	}

	void SubmitRenderPass(OverlayRenderPass* renderPass) {
		assert(!_overlayRenderPass);
		_overlayRenderPass = renderPass;
	}

	void SubmitRenderPass(SunShadowRenderPass* renderPass) {
		assert(!_sunShadowRenderPass);
		_sunShadowRenderPass = renderPass;
	}

	void SubmitRenderPass(SpotShadowRenderPass* renderPass) {
		assert(_numSpotLightShadowRenderPasses < MAX_NUM_SPOT_LIGHT_SHADOW_PASSES);
		_spotLightShadowRenderPasses[_numSpotLightShadowRenderPasses++] = renderPass;
	}

	void SubmitRenderPass(PointShadowRenderPass* renderPass) {
		assert(numPointLightShadowRenderPasses < MAX_NUM_POINT_LIGHT_SHADOW_PASSES);
		_pointLightShadowRenderPasses[numPointLightShadowRenderPasses++] = renderPass;
	}

	void SetRaytracer(DxRaytracer* raytracer, RaytracingTlas* tlas) {
		_raytracer = raytracer;
		_tlas = tlas;
	}

	RendererMode Mode = ERendererModeRasterized;
	RenderSettings Settings;

	uint32 RenderWidth;
	uint32 RenderHeight;
	Ptr<DxTexture> FrameResult;
	Ptr<DxTexture> RenderTarget;

	uint16 HoveredObjectID = 0xffff;

	Ptr<DxTexture> GetWhiteTexture();
	Ptr<DxTexture> GetBlackTexture();
	Ptr<DxTexture> GetShadowMap();

	DxCpuDescriptorHandle NullTextureSRV;
	DxCpuDescriptorHandle NullBufferSRV;

	DXGI_FORMAT ScreenFormat;

	static constexpr DXGI_FORMAT hdrFormat = DXGI_FORMAT_R32G32B32A32_FLOAT; // TODO: This could be way less. However, for path tracing accumulation over time this was necessary.
	static constexpr DXGI_FORMAT worldNormalsFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT hdrDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static constexpr DXGI_FORMAT linearDepthFormat = DXGI_FORMAT_R32_FLOAT;
	static constexpr DXGI_FORMAT screenVelocitiesFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT objectIDsFormat = DXGI_FORMAT_R16_UINT; // Do not change this. 16 bit is hardcoded in other places.
	static constexpr DXGI_FORMAT reflectanceFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Fresnel (xyz), roughness (w).
	static constexpr DXGI_FORMAT reflectionFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT shadowDepthFormat = DXGI_FORMAT_D16_UNORM;
	static constexpr DXGI_FORMAT volumetricsFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	static constexpr DXGI_FORMAT hdrPostProcessFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT ldrPostProcessFormat = DXGI_FORMAT_R11G11B10_FLOAT; // Not really LDR. But I don't like the idea of converting to 8 bit and then to sRGB in separate passes.

	static constexpr DXGI_FORMAT overlayFormat = ldrPostProcessFormat;
	static constexpr DXGI_FORMAT overlayDepthFormat = hdrDepthStencilFormat;

	static constexpr DXGI_FORMAT opaqueLightPassFormats[] = { hdrFormat, worldNormalsFormat, reflectanceFormat };
	static constexpr DXGI_FORMAT transparentLightPassFormats[] = { hdrPostProcessFormat };
	static constexpr DXGI_FORMAT skyPassFormats[] = { hdrFormat, screenVelocitiesFormat, objectIDsFormat };

private:
	static DxRenderer* _instance;

	bool _performedSkinning;
	vec2 _haltonSequence[128];

	uint32 _windowWidth;
	uint32 _windowHeight;
	uint32 _windowXOffset;
	uint32 _windowYOffset;

	DxPipeline _textureSkyPipeline;
	DxPipeline _proceduralSkyPipeline;

	DxPipeline _atmospherePipeline;

	DxPipeline _depthOnlyPipeline;
	DxPipeline _animatedDepthOnlyPipeline;
	DxPipeline _shadowPipeline;
	DxPipeline _pointLightShadowPipeline;

	DxPipeline _outlineMarkerPipeline;
	DxPipeline _outlineDrawerPipline;

	DxPipeline _worldSpaceFrustumPipeline;
	DxPipeline _lightCullingPipeline;

	DxPipeline _ssrRaycastPipeline;
	DxPipeline _ssrResolvePipeline;
	DxPipeline _ssrTemporalPipline;
	DxPipeline _ssrMedianBlurPipeline;

	DxPipeline _taaPipeline;
	DxPipeline _bloomThresholdPipline;
	DxPipeline _bloomCombinePipeline;
	DxPipeline _gaussianBlur9x9Pipline;
	DxPipeline _gaussianBlur5x5Pipeline;
	DxPipeline _specularAmbientPipeline;
	DxPipeline _blitPipeline;
	DxPipeline _hierarchicalLinearDepthPipline;
	DxPipeline _tonemapPipeline;
	DxPipeline _presentPipeline;

	enum EStencilFlags {
		EStencilFlagSelectedObject = (1 << 0)
	};

	Ptr<DxTexture> _whiteTexture;
	Ptr<DxTexture> _blackTexture;
	Ptr<DxTexture> _blackCubeTexture;
	Ptr<DxTexture> _noiseTexture;

	Ptr<DxTexture> _shadowMap;

	Ptr<DxTexture> _brdfTex;

	float _clearColor[4];

	DxRaytracer* _raytracer;
	RaytracingTlas* _tlas;

	OpaqueRenderPass* _opaqueRenderPass;
	TransparentRenderPass* _transparentRenderPass;
	SunShadowRenderPass* _sunShadowRenderPass;
	SpotShadowRenderPass* _spotLightShadowRenderPasses[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
	PointShadowRenderPass* _pointLightShadowRenderPasses[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
	uint32 _numSpotLightShadowRenderPasses;
	uint32 numPointLightShadowRenderPasses;
	OverlayRenderPass* _overlayRenderPass;

	Ptr<DxTexture> _hdrColorTexture;
	Ptr<DxTexture> _depthStencilBuffer;
	Ptr<DxTexture> _worldNormalsTexture;
	Ptr<DxTexture> _screenVelocitiesTexture;
	Ptr<DxTexture> _objectIDsTexture;
	Ptr<DxTexture> _reflectanceTexture;
	Ptr<DxTexture> _linearDepthBuffer;
	Ptr<DxTexture> _opaqueDepthBuffer; // The depth-stencil buffer gets copied to this texture after the opaque pass.

	Ptr<DxTexture> _ssrRaycastTexture;
	Ptr<DxTexture> _ssrResolveTexture;
	Ptr<DxTexture> _ssrTemporalTextures[2];
	uint32 _ssrHistoryIndex = 0;

	Ptr<DxTexture> _prevFrameHDRColorTexture; // This is downsampled by a factor of 2 and contains up to 8 mip levels.
	Ptr<DxTexture> _prevFrameHDRColorTempTexture;

	Ptr<DxTexture> _hdrPostProcessingTexture;
	Ptr<DxTexture> _ldrPostProcessingTexture;
	Ptr<DxTexture> _taaTextures[2]; // These get flip-flopped from frame to frame.
	uint32 _taaHistoryIndex = 0;

	Ptr<DxTexture> _bloomTexture;
	Ptr<DxTexture> _bloomTempTexture;

	Ptr<DxBuffer> _hoveredObjectIDReadbackBuffer;

	Ptr<DxBuffer> _spotLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];
	Ptr<DxBuffer> _pointLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];

	Ptr<PbrEnvironment> _environment;
	Ptr<DxBuffer> _pointLights;
	Ptr<DxBuffer> _spotLights;
	Ptr<DxBuffer> _decals;
	uint32 _numPointLights;
	uint32 _numSpotLights;
	uint32 _numDecals;

	Ptr<DxTexture> _decalTextureAtlas;

	CameraCb _jitteredCamera;
	CameraCb _unjitteredCamera;
	DirectionalLightCb _sun;

	// Tiled light and decal culling.
	Ptr<DxBuffer> _tiledWorldSpaceFrustaBuffer;

	Ptr<DxBuffer> _tiledCullingIndexCounter;
	Ptr<DxBuffer> _tiledObjectsIndexList;

	// DXGI_FORMAT_R32G32B32A32_UINT.
	// The R&B channel contains the offset into tiledObjectsIndexList.
	// The G&A channel contains the number of point lights and spot lights in 10 bit each, so there is space for more info.
	// Opaque is in R,G.
	// Transparent is in B,A.
	// For more info, see light_culling_cs.hlsl.
	Ptr<DxTexture> _tiledCullingGrid;

	uint32 _numCullingTilesX;
	uint32 _numCullingTilesY;

	RenderSettings _oldSettings;
	RendererMode _oldMode = ERendererModeRasterized;

	void RecalculateViewport(bool resizeTextures);
	void AllocateLightCullingBuffers();

	enum GaussianBlurKernelSize
	{
		EGaussian_Blur_5x5,
		EGaussian_Blur_9x9,
	};

	void GaussianBlur(DxCommandList* cl, Ptr<DxTexture> inputOutput, Ptr<DxTexture> temp, uint32 inputMip, uint32 outputMip, GaussianBlurKernelSize kernel, uint32 numIterations = 1);
	void SpecularAmbient(DxCommandList* cl, DxDynamicConstantBuffer cameraCBV, const Ptr<DxTexture>& hdrInput, const Ptr<DxTexture>& ssr, const Ptr<DxTexture>& output);
	void TonemapAndPresent(DxCommandList* cl, const Ptr<DxTexture>& hdrResult);
	void Tonemap(DxCommandList* cl, const Ptr<DxTexture>& hdrResult, D3D12_RESOURCE_STATES transitionLDR);
	void Present(DxCommandList* cl);
};