#include "DxRenderer.h"
#include "DxContext.h"
#include "DxCommandList.h"
#include "DxRenderTarget.h"
#include "DirectXColors.h"
#include "../physics/geometry.h"
#include "DxTexture.h"
#include "DxBarrierBatcher.h"
#include "../render/TexturePreprocessing.h"
#include "../animation/skinning.h"
#include "DxContext.h"
#include "DxProfiling.h"
#include "../core/random.h"

#include "depth_only_rs.hlsli"
#include "outline_rs.hlsli"
#include "sky_rs.hlsli"
#include "light_culling_rs.hlsli"
#include "camera.hlsli"
#include "transform.hlsli"
#include "../render/Raytracing.h"

#include <iostream>

#define SSR_RAYCAST_WIDTH (RenderWidth / 2)
#define SSR_RAYCAST_HEIGHT (RenderHeight / 2)

#define SSR_RESOLVE_WIDTH (RenderWidth / 2)
#define SSR_RESOLVE_HEIGHT (RenderHeight / 2)

namespace {
	float AcesFilmic(float x, float A, float B, float C, float D, float E, float F) {
		return (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) - E / F;
	}

	float FilmicTonemapping(float color, TonemapCb tonemap) {
		float expExposure = exp2(tonemap.Exposure);
		color *= expExposure;

		float r = AcesFilmic(color, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F) /
			AcesFilmic(tonemap.LinearWhite, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F);

		return r;
	}
}

DxRenderer* DxRenderer::_instance = new DxRenderer{};

void DxRenderer::Initialize(DXGI_FORMAT screenFormat, uint32 width, uint32 height, bool renderObjectIDs) {
	ScreenFormat = screenFormat;

	DxContext& dxContext = DxContext::Instance();
	TextureFactory* textureFactory = TextureFactory::Instance();
	{
		uint8 white[] = { 255, 255, 255, 255 };
		_whiteTexture = textureFactory->CreateTexture(white, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(_whiteTexture->Resource, "white");
	}
	{
		uint8 black[] = { 0, 0, 0, 255 };
		_blackTexture = textureFactory->CreateTexture(black, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(_blackTexture->Resource, "Black");

		_blackCubeTexture = textureFactory->CreateTexture(black, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(_blackCubeTexture->Resource, "Black cube");
	}
	{
		_noiseTexture = textureFactory->LoadTextureFromFile("assets/textures/blue_noise.png", ETextureLoadFlagsNoncolor); // Already compressed and in DDS format.
	}

	FrameResult = textureFactory->CreateTexture(nullptr, width, height, screenFormat, false, true, true);
	NullTextureSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateNullTextureSRV();
	NullBufferSRV = dxContext.DescriptorAllocatorCPU().GetFreeHandle().CreateNullBufferSRV();

	TexturePreprocessor* texturePreprocessor = TexturePreprocessor::Instance();
	texturePreprocessor->Initialize();
	InitializeSkinning();

	_shadowMap = textureFactory->CreateDepthTexture(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, shadowDepthFormat);
	SET_NAME(_shadowMap->Resource, "Shadow map");

	DxPipelineFactory* pipelineFactory = DxPipelineFactory::Instance();
	// Sky.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.RenderTargets(skyPassFormats, arraysize(skyPassFormats), hdrDepthStencilFormat)
			.DepthSettings(true, false)
			.CullFrontFaces();

		_proceduralSkyPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "sky_vs", "sky_procedural_ps" });
		_textureSkyPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "sky_vs", "sky_texture_ps" });
	}

	// Depth prepass.
	{
		DXGI_FORMAT depthOnlyFormat[] = { screenVelocitiesFormat, objectIDsFormat };

		auto desc = CREATE_GRAPHICS_PIPELINE
			.RenderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), hdrDepthStencilFormat)
			.InputLayout(inputLayoutPositionUvNormalTangent);

		_depthOnlyPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, ERsInVertexShader);
		_animatedDepthOnlyPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "depth_only_animated_vs", "depth_only_ps" }, ERsInVertexShader);
	}

	// Shadow.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.RenderTargets(0, 0, shadowDepthFormat)
			.InputLayout(inputLayoutPositionUvNormalTangent)
			//.cullFrontFaces()
			;

		_shadowPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "shadow_vs" }, ERsInVertexShader);
		_pointLightShadowPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, ERsInVertexShader);
	}

	// Outline.
	{
		auto markerDesc = CREATE_GRAPHICS_PIPELINE
			.InputLayout(inputLayoutPositionUvNormalTangent)
			.RenderTargets(0, 0, hdrDepthStencilFormat)
			.StencilSettings(D3D12_COMPARISON_FUNC_ALWAYS,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_KEEP,
				D3D12_DEFAULT_STENCIL_READ_MASK,
				EStencilFlagSelectedObject) // Mark selected object.
			.DepthSettings(false, false);

		_outlineMarkerPipeline = pipelineFactory->CreateReloadablePipeline(markerDesc, { "outline_vs" }, ERsInVertexShader);


		auto drawerDesc = CREATE_GRAPHICS_PIPELINE
			.RenderTargets(ldrPostProcessFormat, hdrDepthStencilFormat)
			.StencilSettings(D3D12_COMPARISON_FUNC_EQUAL,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				EStencilFlagSelectedObject, // Read only selected object bit.
				0)
			.DepthSettings(false, false);

		_outlineDrawerPipline = pipelineFactory->CreateReloadablePipeline(drawerDesc, { "fullscreen_triangle_vs", "outline_ps" });
	}

	// Light culling.
	{
		_worldSpaceFrustumPipeline = pipelineFactory->CreateReloadablePipeline("world_space_tiled_frustum_cs");
		_lightCullingPipeline = pipelineFactory->CreateReloadablePipeline("light_culling_cs");
	}

	// Atmosphere.
	{
		_atmospherePipeline = pipelineFactory->CreateReloadablePipeline("atmosphere_cs");
	}

	// Post processing.
	{
		_taaPipeline = pipelineFactory->CreateReloadablePipeline("taa_cs");
		_bloomThresholdPipline = pipelineFactory->CreateReloadablePipeline("bloom_threshold_cs");
		_bloomCombinePipeline = pipelineFactory->CreateReloadablePipeline("bloom_combine_cs");
		_gaussianBlur9x9Pipline = pipelineFactory->CreateReloadablePipeline("gaussian_blur_9x9_cs");
		_gaussianBlur5x5Pipeline = pipelineFactory->CreateReloadablePipeline("gaussian_blur_5x5_cs");
		_blitPipeline = pipelineFactory->CreateReloadablePipeline("blit_cs");
		_specularAmbientPipeline = pipelineFactory->CreateReloadablePipeline("specular_ambient_cs");
		_hierarchicalLinearDepthPipline = pipelineFactory->CreateReloadablePipeline("hierarchical_linear_depth_cs");
		_tonemapPipeline = pipelineFactory->CreateReloadablePipeline("tonemap_cs");
		_presentPipeline = pipelineFactory->CreateReloadablePipeline("present_cs");
	}

	// SSR.
	{
		_ssrRaycastPipeline = pipelineFactory->CreateReloadablePipeline("ssr_raycast_cs");
		_ssrResolvePipeline = pipelineFactory->CreateReloadablePipeline("ssr_resolve_cs");
		_ssrTemporalPipline = pipelineFactory->CreateReloadablePipeline("ssr_temporal_cs");
		_ssrMedianBlurPipeline = pipelineFactory->CreateReloadablePipeline("ssr_median_blur_cs");
	}

	PbrMaterial::InitializePipeline();

	pipelineFactory->CreateAllPendingReloadablePipelines();

	{
		DxCommandList* cl = dxContext.GetFreeRenderCommandList();
		_brdfTex = texturePreprocessor->IntegrateBRDF(cl);
		dxContext.ExecuteCommandList(cl);
	}

	for (uint32 i = 0; i < std::size(_haltonSequence); i++) {
		_haltonSequence[i] = Halton23(i) * 2.f - vec2(1.f);
	}

	_windowWidth = width;
	_windowHeight = height;

	RecalculateViewport(false);

	_hdrColorTexture = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, hdrFormat, false, true, true, D3D12_RESOURCE_STATE_RENDER_TARGET);

	D3D12_RESOURCE_DESC prevFrameHDRColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(hdrFormat, RenderWidth / 2, RenderHeight / 2, 1,
		8, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	_prevFrameHDRColorTexture = textureFactory->CreateTexture(prevFrameHDRColorDesc, 0, 0, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_prevFrameHDRColorTempTexture = textureFactory->CreateTexture(prevFrameHDRColorDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_prevFrameHDRColorTexture->AllocateMipUAVs();
	_prevFrameHDRColorTempTexture->AllocateMipUAVs();

	_worldNormalsTexture = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, worldNormalsFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_screenVelocitiesTexture = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, screenVelocitiesFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_reflectanceTexture = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, reflectanceFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);

	_depthStencilBuffer = textureFactory->CreateDepthTexture(RenderWidth, RenderHeight, hdrDepthStencilFormat);
	_opaqueDepthBuffer = textureFactory->CreateDepthTexture(RenderWidth, RenderHeight, hdrDepthStencilFormat, 1, D3D12_RESOURCE_STATE_COPY_DEST);
	D3D12_RESOURCE_DESC linearDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(linearDepthFormat, RenderWidth, RenderHeight, 1,
		6, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	_linearDepthBuffer = textureFactory->CreateTexture(linearDepthDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_linearDepthBuffer->AllocateMipUAVs();

	_ssrRaycastTexture = textureFactory->CreateTexture(0, SSR_RAYCAST_WIDTH, SSR_RESOLVE_HEIGHT, reflectionFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_ssrResolveTexture = textureFactory->CreateTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, reflectionFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_ssrTemporalTextures[_ssrHistoryIndex] = textureFactory->CreateTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, reflectionFormat, false, false, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_ssrTemporalTextures[1 - _ssrHistoryIndex] = textureFactory->CreateTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, reflectionFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	_hdrPostProcessingTexture = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, hdrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	_taaTextures[_taaHistoryIndex] = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, hdrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_taaTextures[1 - _taaHistoryIndex] = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, hdrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	_ldrPostProcessingTexture = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, ldrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_DESC bloomDesc = CD3DX12_RESOURCE_DESC::Tex2D(hdrPostProcessFormat, RenderWidth / 4, RenderHeight / 4, 1,
		5, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	_bloomTexture = textureFactory->CreateTexture(bloomDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_bloomTempTexture = textureFactory->CreateTexture(bloomDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_bloomTexture->AllocateMipUAVs();
	_bloomTempTexture->AllocateMipUAVs();

	SET_NAME(_hdrColorTexture->Resource, "HDR Color");
	SET_NAME(_prevFrameHDRColorTexture->Resource, "Prev frame HDR Color");
	SET_NAME(_prevFrameHDRColorTempTexture->Resource, "Prev frame HDR Color Temp");
	SET_NAME(_worldNormalsTexture->Resource, "World normals");
	SET_NAME(_screenVelocitiesTexture->Resource, "Screen velocities");
	SET_NAME(_reflectanceTexture->Resource, "Reflectance");
	SET_NAME(_depthStencilBuffer->Resource, "Depth buffer");
	SET_NAME(_opaqueDepthBuffer->Resource, "Opaque depth buffer");
	SET_NAME(_linearDepthBuffer->Resource, "Linear depth buffer");

	SET_NAME(_ssrRaycastTexture->Resource, "SSR Raycast");
	SET_NAME(_ssrResolveTexture->Resource, "SSR Resolve");
	SET_NAME(_ssrTemporalTextures[0]->Resource, "SSR Temporal 0");
	SET_NAME(_ssrTemporalTextures[1]->Resource, "SSR Temporal 1");

	SET_NAME(_taaTextures[0]->Resource, "TAA 0");
	SET_NAME(_taaTextures[1]->Resource, "TAA 1");

	SET_NAME(_hdrPostProcessingTexture->Resource, "HDR Post processing");
	SET_NAME(_ldrPostProcessingTexture->Resource, "LDR Post processing");

	SET_NAME(_bloomTexture->Resource, "Bloom");
	SET_NAME(_bloomTempTexture->Resource, "Bloom Temp");

	SET_NAME(FrameResult->Resource, "Frame result");

	if (renderObjectIDs) {
		_hoveredObjectIDReadbackBuffer = DxBuffer::CreateReadback(DxTexture::GetFormatSize(objectIDsFormat), NUM_BUFFERED_FRAMES);

		_objectIDsTexture = textureFactory->CreateTexture(0, RenderWidth, RenderHeight, objectIDsFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
		SET_NAME(_objectIDsTexture->Resource, "Object IDs");
	}

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i) {
		_spotLightShadowInfoBuffer[i] = DxBuffer::CreateUpload(sizeof(SpotShadowInfo), 16, 0);
		_pointLightShadowInfoBuffer[i] = DxBuffer::CreateUpload(sizeof(PointShadowInfo), 16, 0);

		SET_NAME(_spotLightShadowInfoBuffer[i]->Resource, "Spot light shadow infos");
		SET_NAME(_pointLightShadowInfoBuffer[i]->Resource, "Point light shadow infos");
	}

	memcpy(&_clearColor, DirectX::Colors::LightSteelBlue, sizeof(float) * 4);
}

void DxRenderer::BeginFrameCommon() {
	DxPipelineFactory::Instance()->CheckForChangedPipelines();
}

void DxRenderer::EndFrameCommon() {
	_performedSkinning = PerformSkinning();
}

void DxRenderer::BeginFrame(uint32 width, uint32 height) {
	_pointLights = nullptr;
	_spotLights = nullptr;
	_numPointLights = 0;
	_numSpotLights = 0;
	_decals = nullptr;
	_numDecals = 0;
	_decalTextureAtlas = nullptr;

	if (_windowWidth != width or _windowHeight != height) {
		_windowWidth = width;
		_windowHeight = height;

		// Frame result.
		FrameResult->Resize(width, height);

		RecalculateViewport(true);
	}

	DxContext& dxContext = DxContext::Instance();
	if (_objectIDsTexture) {
		uint16* id = (uint16*)_hoveredObjectIDReadbackBuffer->Map(true, MapRange{ (uint32)dxContext.BufferedFrameId(), 1 });
		HoveredObjectID = *id;
		_hoveredObjectIDReadbackBuffer->Unmap(false);
	}

	_opaqueRenderPass = nullptr;
	_overlayRenderPass = nullptr;
	_transparentRenderPass = nullptr;
	_sunShadowRenderPass = nullptr;
	_numSpotLightShadowRenderPasses = 0;
	numPointLightShadowRenderPasses = 0;

	_pointLights = 0;
	_spotLights = 0;
	_numPointLights = 0;
	_numSpotLights = 0;

	_environment = nullptr;
}

void DxRenderer::RecalculateViewport(bool resizeTextures) {
	if (Settings.AspectRatio == EAspectRatioFree) {
		_windowXOffset = 0;
		_windowYOffset = 0;
		RenderWidth = _windowWidth;
		RenderHeight = _windowHeight;
	}
	else {
		const float targetAspect = Settings.AspectRatio == EAspectRatioFix16_9 ? (16.f / 9.f) : (16.f / 10.f);

		float aspect = (float)_windowWidth / (float)_windowHeight;
		if (aspect > targetAspect) {
			RenderWidth = (uint32)(_windowHeight * targetAspect);
			RenderHeight = _windowHeight;
			_windowXOffset = (_windowWidth - RenderWidth) / 2;
			_windowYOffset = 0;
		}
		else {
			RenderWidth = _windowWidth;
			RenderHeight = (uint32)(_windowWidth / targetAspect);
			_windowXOffset = 0;
			_windowYOffset = (_windowHeight - RenderHeight) / 2;
		}
	}


	if (resizeTextures) {
		_hdrColorTexture->Resize(RenderWidth, RenderHeight);
		_prevFrameHDRColorTexture->Resize(RenderWidth / 2, RenderHeight / 2);
		_prevFrameHDRColorTempTexture->Resize(RenderWidth / 2, RenderHeight / 2);
		_worldNormalsTexture->Resize(RenderWidth, RenderHeight);
		_screenVelocitiesTexture->Resize(RenderWidth, RenderHeight);
		_reflectanceTexture->Resize(RenderWidth, RenderHeight);
		_depthStencilBuffer->Resize(RenderWidth, RenderHeight);
		_opaqueDepthBuffer->Resize(RenderWidth, RenderHeight);
		_linearDepthBuffer->Resize(RenderWidth, RenderHeight);

		if (_objectIDsTexture) {
			_objectIDsTexture->Resize(RenderWidth, RenderHeight);
		}

		_ssrRaycastTexture->Resize(SSR_RAYCAST_WIDTH, SSR_RAYCAST_HEIGHT);
		_ssrResolveTexture->Resize(SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT);
		_ssrTemporalTextures[_ssrHistoryIndex]->Resize(SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		_ssrTemporalTextures[1 - _ssrHistoryIndex]->Resize(SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		_hdrPostProcessingTexture->Resize(RenderWidth, RenderHeight);

		_taaTextures[_taaHistoryIndex]->Resize(RenderWidth, RenderHeight, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		_taaTextures[1 - _taaHistoryIndex]->Resize(RenderWidth, RenderHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		_bloomTexture->Resize(RenderWidth / 4, RenderHeight / 4);
		_bloomTempTexture->Resize(RenderWidth / 4, RenderHeight / 4);

		_ldrPostProcessingTexture->Resize(RenderWidth, RenderHeight);
	}

	AllocateLightCullingBuffers();
}

void DxRenderer::AllocateLightCullingBuffers() {
	_numCullingTilesX = bucketize(RenderWidth, LIGHT_CULLING_TILE_SIZE);
	_numCullingTilesY = bucketize(RenderHeight, LIGHT_CULLING_TILE_SIZE);

	bool firstAllocation = _tiledCullingGrid == nullptr;

	TextureFactory* textureFactory = TextureFactory::Instance();
	if (firstAllocation) {
		_tiledCullingGrid = textureFactory->CreateTexture(0, _numCullingTilesX, _numCullingTilesY,
			DXGI_FORMAT_R32G32B32A32_UINT, false, false, true);

		_tiledCullingIndexCounter = DxBuffer::Create(sizeof(uint32), 1, 0, true, true);
		_tiledObjectsIndexList = DxBuffer::Create(sizeof(uint16), _numCullingTilesX * _numCullingTilesY * MAX_NUM_INDICES_PER_TILE * 2, 0, true);
		_tiledWorldSpaceFrustaBuffer = DxBuffer::Create(sizeof(LightCullingViewFrustum), _numCullingTilesX * _numCullingTilesY, 0, true);

		SET_NAME(_tiledCullingGrid->Resource, "Tiled culling grid");
		SET_NAME(_tiledCullingIndexCounter->Resource, "Tiled index counter");
		SET_NAME(_tiledObjectsIndexList->Resource, "Tiled index list");
		SET_NAME(_tiledWorldSpaceFrustaBuffer->Resource, "Tiled frusta");
	}
	else {
		_tiledCullingGrid->Resize(_numCullingTilesX, _numCullingTilesY);
		_tiledObjectsIndexList->Resize(_numCullingTilesX * _numCullingTilesY * MAX_NUM_INDICES_PER_TILE * 2);
		_tiledWorldSpaceFrustaBuffer->Resize(_numCullingTilesX * _numCullingTilesY);
	}
}

void DxRenderer::GaussianBlur(DxCommandList *cl, Ptr<DxTexture> inputOutput, Ptr<DxTexture> temp, uint32 inputMip, uint32 outputMip, GaussianBlurKernelSize kernel, uint32 numIterations) {
	DX_PROFILE_BLOCK(cl, "Gaussian Blur");

	auto& pipeline =
		(kernel == EGaussian_Blur_5x5) ? _gaussianBlur5x5Pipeline :
		(kernel == EGaussian_Blur_9x9) ? _gaussianBlur9x9Pipline :
		_gaussianBlur9x9Pipline; // TODO: Emit error!

	cl->SetPipelineState(*pipeline.Pipeline);
	cl->SetComputeRootSignature(*pipeline.RootSignature);

	uint32 outputWidth = inputOutput->Width >> outputMip;
	uint32 outputHeight = inputOutput->Height >> outputMip;

	uint32 widthBuckets = bucketize(outputWidth, POST_PROCESSING_BLOCK_SIZE);
	uint32 heightBuckets = bucketize(outputHeight, POST_PROCESSING_BLOCK_SIZE);

	assert((outputMip == 0) || ((uint32)inputOutput->MipUAVs.size() >= outputMip));
	assert((outputMip == 0) || ((uint32)temp->MipUAVs.size() >= outputMip));
	assert(inputMip <= outputMip); // Currently only downsampling supported.

	float scale = 1.f / (1 << (outputMip - inputMip));

	uint32 sourceMip = inputMip;
	GaussianBlurCb cb = { vec2(1.f / outputWidth, 1.f / outputHeight), scale };

	for (uint32 i = 0; i < numIterations; ++i) {
		DX_PROFILE_BLOCK(cl, "Iteration");

		{
			DX_PROFILE_BLOCK(cl, "Vertical");

			DxCpuDescriptorHandle tempUAV = (outputMip == 0) ? temp->DefaultUAV : temp->MipUAVs[outputMip - 1];

			// Vertical pass.
			cb.DirectionAndSourceMipLevel = (1 << 16) | sourceMip;
			cl->SetCompute32BitConstants(GaussianBlurRsCb, cb);
			cl->SetDescriptorHeapUAV(GaussianBlurRsTextures, 0, tempUAV);
			cl->SetDescriptorHeapSRV(GaussianBlurRsTextures, 1, inputOutput);

			cl->Dispatch(widthBuckets, heightBuckets);

			DxBarrierBatcher(cl)
				.UAV(temp)
				.Transition(temp, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.Transition(inputOutput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		cb.StepScale = 1.f;
		sourceMip = outputMip; // From here on we sample from the output mip.

		{
			DX_PROFILE_BLOCK(cl, "Horizontal");

			DxCpuDescriptorHandle outputUAV = (outputMip == 0) ? inputOutput->DefaultUAV : inputOutput->MipUAVs[outputMip - 1];

			// Horizontal pass.
			cb.DirectionAndSourceMipLevel = (0 << 16) | sourceMip;
			cl->SetCompute32BitConstants(GaussianBlurRsCb, cb);
			cl->SetDescriptorHeapUAV(GaussianBlurRsTextures, 0, outputUAV);
			cl->SetDescriptorHeapSRV(GaussianBlurRsTextures, 1, temp);

			cl->Dispatch(widthBuckets, heightBuckets);

			DxBarrierBatcher(cl)
				.UAV(inputOutput)
				.Transition(temp, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.Transition(inputOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
	}
}

void DxRenderer::SpecularAmbient(DxCommandList *cl, DxDynamicConstantBuffer cameraCBV, const Ptr<DxTexture> &hdrInput, const Ptr<DxTexture> &ssr, const Ptr<DxTexture> &output) {
	cl->SetPipelineState(*_specularAmbientPipeline.Pipeline);
	cl->SetComputeRootSignature(*_specularAmbientPipeline.RootSignature);

	cl->SetCompute32BitConstants(SpecularAmbientRsCb, SpecularAmbientCb{ vec2(1.f / RenderWidth, 1.f / RenderHeight) });
	cl->SetComputeDynamicConstantBuffer(SpecularAmbientRsCamera, cameraCBV);

	cl->SetDescriptorHeapUAV(SpecularAmbientRsTextures, 0, output);
	cl->SetDescriptorHeapSRV(SpecularAmbientRsTextures, 1, hdrInput);
	cl->SetDescriptorHeapSRV( SpecularAmbientRsTextures, 2, _worldNormalsTexture);
	cl->SetDescriptorHeapSRV( SpecularAmbientRsTextures, 3, _reflectanceTexture);
	cl->SetDescriptorHeapSRV(SpecularAmbientRsTextures, 4, ssr ? ssr->DefaultSRV : NullTextureSRV);
	cl->SetDescriptorHeapSRV(SpecularAmbientRsTextures, 5, _environment->Environment);
	cl->SetDescriptorHeapSRV(SpecularAmbientRsTextures, 6, _brdfTex);

	cl->Dispatch(bucketize(RenderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(RenderHeight, POST_PROCESSING_BLOCK_SIZE));
}

void DxRenderer::TonemapAndPresent(DxCommandList *cl, const Ptr<DxTexture> &hdrResult) {
	Tonemap(cl, hdrResult, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Present(cl);
}

void DxRenderer::Tonemap(DxCommandList *cl, const Ptr<DxTexture> &hdrResult, D3D12_RESOURCE_STATES transitionLDR) {
	DX_PROFILE_BLOCK(cl, "Tonemapping");

	cl->SetPipelineState(*_tonemapPipeline.Pipeline);
	cl->SetComputeRootSignature(*_tonemapPipeline.RootSignature);

	cl->SetDescriptorHeapUAV(TonemapRsTextures, 0, _ldrPostProcessingTexture);
	cl->SetDescriptorHeapSRV(TonemapRsTextures, 1, hdrResult);
	cl->SetCompute32BitConstants(TonemapRsCb, Settings.Tonemap);

	cl->Dispatch(bucketize(RenderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(RenderHeight, POST_PROCESSING_BLOCK_SIZE));

	DxBarrierBatcher(cl)
		.UAV(_ldrPostProcessingTexture)
		.Transition(_ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, transitionLDR);
}

void DxRenderer::Present(DxCommandList *cl) {
	DX_PROFILE_BLOCK(cl, "Present");

	cl->SetPipelineState(*_presentPipeline.Pipeline);
	cl->SetComputeRootSignature(*_presentPipeline.RootSignature);

	cl->SetDescriptorHeapUAV(PresentRsTextures, 0, FrameResult);
	cl->SetDescriptorHeapSRV(PresentRsTextures, 1, _ldrPostProcessingTexture);
	cl->SetCompute32BitConstants(PresentRsCb, PresentCb{ PRESENT_SDR, 0.f, Settings.SharpenStrength * Settings.EnableSharpen, (_windowXOffset << 16) | _windowYOffset });

	cl->Dispatch(bucketize(RenderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(RenderHeight, POST_PROCESSING_BLOCK_SIZE));
}

void DxRenderer::SetCamera(const RenderCamera &camera) {
	vec2 jitterOffset(0.f, 0.f);
	RenderCamera c;
	DxContext& dxContext = DxContext::Instance();
	if (Settings.EnableTemporalAntialiasing) {
		jitterOffset = _haltonSequence[dxContext.FrameId() % std::size(_haltonSequence)] / vec2((float)RenderWidth, (float)RenderHeight) * Settings.CameraJitterStrength;
		c = camera.GetJitteredVersion(jitterOffset);
	}
	else {
		c = camera;
	}

	_jitteredCamera.PrevFrameViewProj = _jitteredCamera.ViewProj;
	_jitteredCamera.ViewProj = c.ViewProj;
	_jitteredCamera.View = c.View;
	_jitteredCamera.Proj = c.Proj;
	_jitteredCamera.InvViewProj = c.InvViewProj;
	_jitteredCamera.InvView = c.InvView;
	_jitteredCamera.InvProj = c.InvProj;
	_jitteredCamera.Position = vec4(c.Position, 1.f);
	_jitteredCamera.Forward = vec4(c.Rotation * vec3(0.f, 0.f, -1.f), 0.f);
	_jitteredCamera.Right = vec4(c.Rotation * vec3(1.f, 0.f, 0.f), 0.f);
	_jitteredCamera.Up = vec4(c.Rotation * vec3(0.f, 1.f, 0.f), 0.f);
	_jitteredCamera.ProjectionParams = vec4(c.NearPlane, c.FarPlane, c.FarPlane / c.NearPlane, 1.f - c.FarPlane / c.NearPlane);
	_jitteredCamera.ScreenDims = vec2((float)RenderWidth, (float)RenderHeight);
	_jitteredCamera.InvScreenDims = vec2(1.f / RenderWidth, 1.f / RenderHeight);
	_jitteredCamera.PrevFrameJitter = _jitteredCamera.Jitter;
	_jitteredCamera.Jitter = jitterOffset;


	_unjitteredCamera.PrevFrameViewProj = _unjitteredCamera.ViewProj;
	_unjitteredCamera.ViewProj = camera.ViewProj;
	_unjitteredCamera.View = camera.View;
	_unjitteredCamera.Proj = camera.Proj;
	_unjitteredCamera.InvViewProj = camera.InvViewProj;
	_unjitteredCamera.InvView = camera.InvView;
	_unjitteredCamera.InvProj = camera.InvProj;
	_unjitteredCamera.Position = vec4(camera.Position, 1.f);
	_unjitteredCamera.Forward = vec4(camera.Rotation * vec3(0.f, 0.f, -1.f), 0.f);
	_unjitteredCamera.Right = vec4(camera.Rotation * vec3(1.f, 0.f, 0.f), 0.f);
	_unjitteredCamera.Up = vec4(camera.Rotation * vec3(0.f, 1.f, 0.f), 0.f);
	_unjitteredCamera.ProjectionParams = vec4(camera.NearPlane, camera.FarPlane, camera.FarPlane / camera.NearPlane, 1.f - camera.FarPlane / camera.NearPlane);
	_unjitteredCamera.ScreenDims = vec2((float)RenderWidth, (float)RenderHeight);
	_unjitteredCamera.InvScreenDims = vec2(1.f / RenderWidth, 1.f / RenderHeight);
	_unjitteredCamera.PrevFrameJitter = vec2(0.f);
	_unjitteredCamera.Jitter = vec2(0.f);
}

void DxRenderer::SetEnvironment(const Ptr<PbrEnvironment> &environment) {
	_environment = environment;
}

void DxRenderer::SetSun(const DirectionalLight &light) {
	_sun.CascadeDistances = light.CascadeDistances;
	_sun.Bias = light.Bias;
	_sun.Direction = light.Direction;
	_sun.BlendDistances = light.BlendDistances;
	_sun.Radiance = light.Color * light.Intensity;
	_sun.NumShadowCascades = light.NumShadowCascades;

	memcpy(_sun.ViewProj, light.ViewProj, sizeof(mat4) * light.NumShadowCascades);
}

void DxRenderer::SetPointLights(const Ptr<DxBuffer> &lights, uint32 numLights) {
	_pointLights = lights;
	_numPointLights = numLights;
}

void DxRenderer::SetSpotLights(const Ptr<DxBuffer> &lights, uint32 numLights) {
	_spotLights = lights;
	_numSpotLights = numLights;
}

void DxRenderer::SetDecals(const Ptr<DxBuffer> &decals, uint32 numDecals, const Ptr<DxTexture> &textureAtlas) {
	assert(_numDecals < MAX_NUM_TOTAL_DECALS);
	_decals = decals;
	_numDecals = numDecals;
	_decalTextureAtlas = textureAtlas;
}

Ptr<DxTexture> DxRenderer::GetWhiteTexture() {
	return _whiteTexture;
}

Ptr<DxTexture> DxRenderer::GetBlackTexture() {
	return _blackTexture;
}

Ptr<DxTexture> DxRenderer::GetShadowMap() {
	return _shadowMap;
}

void DxRenderer::EndFrame(const UserInput &input) {
	DxContext& dxContext = DxContext::Instance();
	bool aspectRatioModeChanged = Settings.AspectRatio != _oldSettings.AspectRatio;
	_oldSettings = Settings;

	if (aspectRatioModeChanged) {
		RecalculateViewport(true);
	}

	vec4 sunCPUShadowViewports[MAX_NUM_SHADOW_CASCADES];
	vec4 spotLightViewports[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
	vec4 pointLightViewports[MAX_NUM_POINT_LIGHT_SHADOW_PASSES][2];

	{
		SpotShadowInfo spotLightShadowInfos[16];
		PointShadowInfo pointLightShadowInfos[16];

		if (_sunShadowRenderPass) {
			for (uint32 i = 0; i < _sun.NumShadowCascades; ++i) {
				sunCPUShadowViewports[i] = _sunShadowRenderPass->Viewports[i];
				_sun.Viewports[i] = sunCPUShadowViewports[i] / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
			}
		}

		for (uint32 i = 0; i < _numSpotLightShadowRenderPasses; ++i) {
			SpotShadowInfo& si = spotLightShadowInfos[i];

			spotLightViewports[i] = _spotLightShadowRenderPasses[i]->Viewport;
			si.Viewport = spotLightViewports[i] / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
			si.ViewProj = _spotLightShadowRenderPasses[i]->ViewProjMatrix;
			si.Bias = 0.00002f;
		}

		for (uint32 i = 0; i < numPointLightShadowRenderPasses; ++i) {
			PointShadowInfo& si = pointLightShadowInfos[i];

			pointLightViewports[i][0] = _pointLightShadowRenderPasses[i]->Viewport0;
			pointLightViewports[i][1] = _pointLightShadowRenderPasses[i]->Viewport1;

			si.Viewport0 = pointLightViewports[i][0] / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
			si.Viewport1 = pointLightViewports[i][1] / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
		}

		_spotLightShadowInfoBuffer[dxContext.BufferedFrameId()]->UpdateUploadData(spotLightShadowInfos, (uint32)(sizeof(SpotShadowInfo) * _numSpotLightShadowRenderPasses));
		_pointLightShadowInfoBuffer[dxContext.BufferedFrameId()]->UpdateUploadData(pointLightShadowInfos, (uint32)(sizeof(PointShadowInfo) * numPointLightShadowRenderPasses));
	}

	auto jitteredCameraCBV = dxContext.UploadDynamicConstantBuffer(_jitteredCamera);
	auto unjitteredCameraCBV = dxContext.UploadDynamicConstantBuffer(_unjitteredCamera);
	auto sunCBV = dxContext.UploadDynamicConstantBuffer(_sun);

	CommonMaterialInfo materialInfo;
	if (_environment) {
		materialInfo.Sky = _environment->Sky;
		materialInfo.Environment = _environment->Environment;
		materialInfo.Irradiance = _environment->Irradiance;
	}
	else {
		materialInfo.Sky = _blackCubeTexture;
		materialInfo.Environment = _blackCubeTexture;
		materialInfo.Irradiance = _blackCubeTexture;
	}
	materialInfo.EnvironmentIntensity = Settings.EnvironmentIntensity;
	materialInfo.SkyIntensity = Settings.SkyIntensity;
	materialInfo.Brdf = _brdfTex;
	materialInfo.TiledCullingGrid = _tiledCullingGrid;
	materialInfo.TiledObjectsIndexList = _tiledObjectsIndexList;
	materialInfo.PointLightBuffer = _pointLights;
	materialInfo.SpotlightBuffer = _spotLights;
	materialInfo.DecalBuffer = _decals;
	materialInfo.ShadowMap = _shadowMap;
	materialInfo.DecalTextureAtlas = _decalTextureAtlas;
	materialInfo.PointLightShadowInfoBuffer = _pointLightShadowInfoBuffer[dxContext.BufferedFrameId()];
	materialInfo.SpotlightShadowInfoBuffer = _spotLightShadowInfoBuffer[dxContext.BufferedFrameId()];
	materialInfo.VolumetricsTexture = 0;
	materialInfo.CameraCBV = jitteredCameraCBV;
	materialInfo.SunCBV = sunCBV;

	DxCommandList* cl = dxContext.GetFreeRenderCommandList();

	D3D12_RESOURCE_STATES frameResultState = D3D12_RESOURCE_STATE_COMMON;

	if (aspectRatioModeChanged) {
		cl->TransitionBarrier(FrameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cl->ClearRTV(FrameResult, DirectX::Colors::LightSteelBlue);
		frameResultState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	DxBarrierBatcher(cl).TransitionBegin(FrameResult, frameResultState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	if (Mode == ERendererModeRasterized) {
		cl->ClearDepthAndStencil(_depthStencilBuffer->DsvHandle);
		cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (_performedSkinning) {
			dxContext.RenderQueue.WaitForOtherQueue(dxContext.ComputeQueue); // Wait for GPU skinning.
		}

		// ----------------------------------------
		// DEPTH-ONLY PASS
		// ----------------------------------------
		DxRenderTarget depthOnlyRenderTarget({ _screenVelocitiesTexture, _objectIDsTexture }, _depthStencilBuffer);
		cl->SetRenderTarget(depthOnlyRenderTarget);
		cl->SetViewport(depthOnlyRenderTarget.Viewport);
#if 1
		{
			DX_PROFILE_BLOCK(cl, "Depth pre-pass");

			// Static.
			if (_opaqueRenderPass && _opaqueRenderPass->_staticDepthOnlyDrawCalls.size() > 0) {
				DX_PROFILE_BLOCK(cl, "Static");

				cl->SetPipelineState(*_depthOnlyPipeline.Pipeline);
				cl->SetGraphicsRootSignature(*_depthOnlyPipeline.RootSignature);

				cl->SetGraphicsDynamicConstantBuffer(DepthOnlyRsCamera, materialInfo.CameraCBV);

				for (const auto& dc : _opaqueRenderPass->_staticDepthOnlyDrawCalls) {
					const mat4& m = dc.Transform;
					const SubmeshInfo& submesh = dc.Submesh;

					cl->SetGraphics32BitConstants(DepthOnlyRsObjectId, (uint32)dc.ObjectID);
					cl->SetGraphics32BitConstants(DepthOnlyRsMvp, DepthOnlyTransformCb{ _jitteredCamera.ViewProj * m, _jitteredCamera.PrevFrameViewProj * m });

					cl->SetVertexBuffer(0, dc.VertexBuffer);
					cl->SetIndexBuffer(dc.IndexBuffer);
					cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
				}
			}

			// Dynamic.
			if (_opaqueRenderPass && _opaqueRenderPass->_dynamicDepthOnlyDrawCalls.size() > 0) {
				DX_PROFILE_BLOCK(cl, "Dynamic");

				cl->SetPipelineState(*_depthOnlyPipeline.Pipeline);
				cl->SetGraphicsRootSignature(*_depthOnlyPipeline.RootSignature);

				cl->SetGraphicsDynamicConstantBuffer(DepthOnlyRsCamera, materialInfo.CameraCBV);

				for (const auto& dc : _opaqueRenderPass->_dynamicDepthOnlyDrawCalls) {
					const mat4& m = dc.Transform;
					const mat4& prevFrameM = dc.PrevFrameTransform;
					const SubmeshInfo& submesh = dc.Submesh;

					cl->SetGraphics32BitConstants(DepthOnlyRsObjectId, (uint32)dc.ObjectID);
					cl->SetGraphics32BitConstants(DepthOnlyRsMvp, DepthOnlyTransformCb{ _jitteredCamera.ViewProj * m, _jitteredCamera.PrevFrameViewProj * prevFrameM });

					cl->SetVertexBuffer(0, dc.VertexBuffer);
					cl->SetIndexBuffer(dc.IndexBuffer);
					cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
				}
			}

			// Animated.
			if (_opaqueRenderPass && _opaqueRenderPass->_animatedDepthOnlyDrawCalls.size() > 0) {
				DX_PROFILE_BLOCK(cl, "Animated");

				cl->SetPipelineState(*_animatedDepthOnlyPipeline.Pipeline);
				cl->SetGraphicsRootSignature(*_animatedDepthOnlyPipeline.RootSignature);

				cl->SetGraphicsDynamicConstantBuffer(DepthOnlyRsCamera, materialInfo.CameraCBV);

				for (const auto& dc : _opaqueRenderPass->_animatedDepthOnlyDrawCalls) {
					const mat4& m = dc.Transform;
					const mat4& prevFrameM = dc.PrevFrameTransform;
					const SubmeshInfo& submesh = dc.Submesh;
					const SubmeshInfo& prevFrameSubmesh = dc.PrevFrameSubmesh;
					const Ptr<DxVertexBuffer>& prevFrameVertexBuffer = dc.PrevFrameVertexBuffer;

					cl->SetGraphics32BitConstants(DepthOnlyRsObjectId, (uint32)dc.ObjectID);
					cl->SetGraphics32BitConstants(DepthOnlyRsMvp, DepthOnlyTransformCb{ _jitteredCamera.ViewProj * m, _jitteredCamera.PrevFrameViewProj * prevFrameM });
					cl->SetRootGraphicsSRV(DepthOnlyRsPrevFramePositions, prevFrameVertexBuffer->GpuVirtualAddress + prevFrameSubmesh.BaseVertex * prevFrameVertexBuffer->ElementSize);

					cl->SetVertexBuffer(0, dc.VertexBuffer);
					cl->SetIndexBuffer(dc.IndexBuffer);
					cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
				}
			}
		}
		DxBarrierBatcher(cl).Transition(_depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
#endif

		// ----------------------------------------
		// LIGHT & DECAL CULLING
		// ----------------------------------------
#if 1
		if (_numPointLights or _numSpotLights or _numDecals) {
			DX_PROFILE_BLOCK(cl, "Cull lights & decals");

			// Tiled frusta.
			{
				DX_PROFILE_BLOCK(cl, "Create world space frusta");

				cl->SetPipelineState(*_worldSpaceFrustumPipeline.Pipeline);
				cl->SetComputeRootSignature(*_worldSpaceFrustumPipeline.RootSignature);
				cl->SetComputeDynamicConstantBuffer(WorldSpaceTiledFrustumRsCamera, materialInfo.CameraCBV);
				cl->SetCompute32BitConstants(WorldSpaceTiledFrustumRsCb, FrustumCb{ _numCullingTilesX, _numCullingTilesY });
				cl->SetRootComputeUAV(WorldSpaceTiledFrustumRsFrustumUav, _tiledWorldSpaceFrustaBuffer);
				cl->Dispatch(bucketize(_numCullingTilesX, 16), bucketize(_numCullingTilesY, 16));
			}

			// Culling.
			{
				DX_PROFILE_BLOCK(cl, "Sort objects into tiles");

				cl->SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dxContext.DescriptorAllocatorGPU().DescriptorHeap());
				cl->ClearUAV(_tiledCullingIndexCounter, 0.f);
				cl->ResetToDynamicDescriptorHeap();
				cl->SetPipelineState(*_lightCullingPipeline.Pipeline);
				cl->SetComputeRootSignature(*_lightCullingPipeline.RootSignature);
				cl->SetComputeDynamicConstantBuffer(LightCullingRsCamera, materialInfo.CameraCBV);
				cl->SetCompute32BitConstants(LightCullingRsCb, LightCullingCb{ _numCullingTilesX, _numPointLights, _numSpotLights, _numDecals });
				cl->SetDescriptorHeapSRV(LightCullingRsSrvUav, 0, _depthStencilBuffer);
				cl->SetDescriptorHeapSRV(LightCullingRsSrvUav, 1, _tiledWorldSpaceFrustaBuffer);
				cl->SetDescriptorHeapSRV(LightCullingRsSrvUav, 2, _pointLights ? _pointLights->DefaultSRV : NullBufferSRV);
				cl->SetDescriptorHeapSRV(LightCullingRsSrvUav, 3, _spotLights ? _spotLights->DefaultSRV : NullBufferSRV);
				cl->SetDescriptorHeapSRV(LightCullingRsSrvUav, 4, _decals ? _decals->DefaultSRV : NullBufferSRV);
				cl->SetDescriptorHeapUAV(LightCullingRsSrvUav, 5, _tiledCullingGrid);
				cl->SetDescriptorHeapUAV(LightCullingRsSrvUav, 6, _tiledCullingIndexCounter);
				cl->SetDescriptorHeapUAV(LightCullingRsSrvUav, 7, _tiledObjectsIndexList);
				cl->Dispatch(_numCullingTilesX, _numCullingTilesY);
			}

			DxBarrierBatcher(cl).UAV(_tiledCullingGrid).UAV(_tiledObjectsIndexList);
		}
#endif

		// ----------------------------------------
		// LINEAR DEPTH PYRAMID
		// ----------------------------------------
#if 1
		{
			DX_PROFILE_BLOCK(cl, "Linear depth pyramid");

			cl->SetPipelineState(*_hierarchicalLinearDepthPipline.Pipeline);
			cl->SetComputeRootSignature(*_hierarchicalLinearDepthPipline.RootSignature);

			float width = ceilf(RenderWidth * 0.5f);
			float height = ceilf(RenderHeight * 0.5f);

			cl->SetCompute32BitConstants(HierarchicalLinearDepthRsCb, HierarchicalLinearDepthCb{ vec2(1.f / width, 1.f / height) });
			cl->SetComputeDynamicConstantBuffer(HierarchicalLinearDepthRsCamera, materialInfo.CameraCBV);
			cl->SetDescriptorHeapUAV(HierarchicalLinearDepthRsTextures, 0, _linearDepthBuffer);
			cl->SetDescriptorHeapUAV(HierarchicalLinearDepthRsTextures, 1, _linearDepthBuffer->MipUAVs[0]);
			cl->SetDescriptorHeapUAV(HierarchicalLinearDepthRsTextures, 2, _linearDepthBuffer->MipUAVs[1]);
			cl->SetDescriptorHeapUAV(HierarchicalLinearDepthRsTextures, 3, _linearDepthBuffer->MipUAVs[2]);
			cl->SetDescriptorHeapUAV(HierarchicalLinearDepthRsTextures, 4, _linearDepthBuffer->MipUAVs[3]);
			cl->SetDescriptorHeapUAV(HierarchicalLinearDepthRsTextures, 5, _linearDepthBuffer->MipUAVs[4]);
			cl->SetDescriptorHeapSRV(HierarchicalLinearDepthRsTextures, 6, _depthStencilBuffer);

			cl->Dispatch(bucketize((uint32)width, POST_PROCESSING_BLOCK_SIZE), bucketize((uint32)height, POST_PROCESSING_BLOCK_SIZE));

			DxBarrierBatcher(cl)
			.UAV(_linearDepthBuffer)
			.TransitionBegin(_linearDepthBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.Transition(_depthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}
#endif

		// ----------------------------------------
		// SHADOW MAP PASS
		// ----------------------------------------
#if 1
		{
			DX_PROFILE_BLOCK(cl, "Shadow map pass");

			DxRenderTarget shadowRenderTarget({}, _shadowMap);

			cl->ClearDepth(shadowRenderTarget);

			cl->SetPipelineState(*_shadowPipeline.Pipeline);
			cl->SetGraphicsRootSignature(*_shadowPipeline.RootSignature);

			cl->SetRenderTarget(shadowRenderTarget);
			cl->ClearDepth(shadowRenderTarget);

			{
				DX_PROFILE_BLOCK(cl, "Sun");

				if (_sunShadowRenderPass) {
					for (uint32 i = 0; i < _sun.NumShadowCascades; ++i) {
						DX_PROFILE_BLOCK(cl, (i == 0) ? "First cascade" : (i == 1) ? "Second cascade" : (i == 2) ? "Third cascade" : "Fourth cascade");

						vec4 vp = sunCPUShadowViewports[i];
						cl->SetViewport(vp.x, vp.y, vp.z, vp.w);

						for (uint32 cascade = 0; cascade <= i; ++cascade) {
							for (const auto& dc : _sunShadowRenderPass->_drawCalls[cascade]) {
								const mat4& m = dc.Transform;
								const SubmeshInfo& submesh = dc.Submesh;
								cl->SetGraphics32BitConstants(ShadowRsMvp, _sun.ViewProj[i] * m);

								cl->SetVertexBuffer(0, dc.VertexBuffer);
								cl->SetIndexBuffer(dc.IndexBuffer);

								cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
							}
						}
					}
				}
			}

			{
				DX_PROFILE_BLOCK(cl, "Spot lights");

				for (uint32 i = 0; i < _numSpotLightShadowRenderPasses; ++i) {
					DX_PROFILE_BLOCK(cl, "Single light");

					const mat4& viewProj = _spotLightShadowRenderPasses[i]->ViewProjMatrix;

					cl->SetViewport(spotLightViewports[i].x, spotLightViewports[i].y, spotLightViewports[i].z, spotLightViewports[i].w);

					for (const auto& dc : _spotLightShadowRenderPasses[i]->_drawCalls) {
						const mat4& m = dc.Transform;
						const SubmeshInfo& submesh = dc.Submesh;
						cl->SetGraphics32BitConstants(ShadowRsMvp, viewProj * m);

						cl->SetVertexBuffer(0, dc.VertexBuffer);
						cl->SetIndexBuffer(dc.IndexBuffer);

						cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
					}
				}
			}

			cl->SetPipelineState(*_pointLightShadowPipeline.Pipeline);
			cl->SetGraphicsRootSignature(*_pointLightShadowPipeline.RootSignature);

			{
				DX_PROFILE_BLOCK(cl, "Point lights");

				for (uint32 i = 0; i < numPointLightShadowRenderPasses; ++i) {
					DX_PROFILE_BLOCK(cl, "Single light");

					for (uint32 v = 0; v < 2; ++v) {
						DX_PROFILE_BLOCK(cl, (v == 0) ? "First hemisphere" : "Second hemisphere");

						cl->SetViewport(pointLightViewports[i][v].x, pointLightViewports[i][v].y, pointLightViewports[i][v].z, pointLightViewports[i][v].w);

						float flip = (v == 0) ? 1.f : -1.f;

						for (const auto& dc : _pointLightShadowRenderPasses[i]->_drawCalls) {
							const mat4& m = dc.Transform;
							const SubmeshInfo& submesh = dc.Submesh;
							cl->SetGraphics32BitConstants(ShadowRsMvp, PointShadowTransformCb{m, _pointLightShadowRenderPasses[i]->LightPosition, _pointLightShadowRenderPasses[i]->MaxDistance, flip});

							cl->SetVertexBuffer(0, dc.VertexBuffer);
							cl->SetIndexBuffer(dc.IndexBuffer);

							cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
						}
					}
				}
			}
		}
		DxBarrierBatcher(cl).Transition(_shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
#endif

		// ----------------------------------------
		// VOLUMETRICS
		// ----------------------------------------
#if 0
		cl->setPipelineState(*atmospherePipeline.pipeline);
		cl->setComputeRootSignature(*atmospherePipeline.rootSignature);
		cl->setComputeDynamicConstantBuffer(0, cameraCBV);
		cl->setComputeDynamicConstantBuffer(1, sunCBV);
		cl->setDescriptorHeapSRV(2, 0, depthBuffer);
		for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES; ++i)
		{
			cl->setDescriptorHeapSRV(2, i + 1, sunShadowCascadeTextures[i]);
		}
		cl->setDescriptorHeapUAV(2, 5, volumetricsTexture);

		cl->dispatch(bucketize(renderWidth, 16), bucketize(renderHeight, 16));
#endif

		// ----------------------------------------
		// SKY PASS
		// ----------------------------------------
		DxRenderTarget skyRenderTarget({ _hdrColorTexture, _screenVelocitiesTexture, _objectIDsTexture }, _depthStencilBuffer);
		cl->SetRenderTarget(skyRenderTarget);
		cl->SetViewport(skyRenderTarget.Viewport);
#if 1
		{
			DX_PROFILE_BLOCK(cl, "Sky");

			if (_environment) {
				cl->SetPipelineState(*_textureSkyPipeline.Pipeline);
				cl->SetGraphicsRootSignature(*_textureSkyPipeline.RootSignature);

				cl->SetGraphics32BitConstants(SkyRsVp, SkyCb{ _jitteredCamera.Proj * CreateSkyViewMatrix(_jitteredCamera.View) });
				cl->SetGraphics32BitConstants(SkyRsIntensity, SkyIntensityCb{ Settings.SkyIntensity });
				cl->SetDescriptorHeapSRV(SkyRsTex, 0, _environment->Sky->DefaultSRV);

				cl->DrawCubeTriangleStrip();
			}
			else {
				cl->SetPipelineState(*_proceduralSkyPipeline.Pipeline);
				cl->SetGraphicsRootSignature(*_proceduralSkyPipeline.RootSignature);

				cl->SetGraphics32BitConstants(SkyRsVp, SkyCb{ _jitteredCamera.Proj * CreateSkyViewMatrix(_jitteredCamera.View) });
				cl->SetGraphics32BitConstants(SkyRsIntensity, SkyIntensityCb{ Settings.SkyIntensity });

				cl->DrawCubeTriangleStrip();
			}

			cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Cube renders a triangle strip, so reset back to triangle list.
		}
		// Copy hovered object id to readback buffer.
		if (_objectIDsTexture) {
			DX_PROFILE_BLOCK(cl, "Copy hovered object ID");

			DxBarrierBatcher(cl).Transition(_objectIDsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);

			if (input.OverWindow) {
				cl->CopyTextureRegionToBuffer(_objectIDsTexture, _hoveredObjectIDReadbackBuffer, dxContext.BufferedFrameId(), (uint32)input.Mouse.X, (uint32)input.Mouse.Y, 1, 1);
			}

			DxBarrierBatcher(cl).TransitionBegin(_objectIDsTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
#endif

		// ----------------------------------------
		// OPAQUE LIGHT PASS
		// ----------------------------------------
		DxRenderTarget hdrOpaqueRenderTarget({ _hdrColorTexture, _worldNormalsTexture, _reflectanceTexture }, _depthStencilBuffer);
		cl->SetRenderTarget(hdrOpaqueRenderTarget);
		cl->SetViewport(hdrOpaqueRenderTarget.Viewport);
#if 1
		if (_opaqueRenderPass && _opaqueRenderPass->_drawCalls.size() > 0) {
			DX_PROFILE_BLOCK(cl, "Main opaque light pass");

			MaterialSetupFunction lastSetupFunc = 0;

			for (const auto& dc : _opaqueRenderPass->_drawCalls)
			{
				const mat4& m = dc.Transform;
				const SubmeshInfo& submesh = dc.Submesh;

				if (dc.MaterialSetup != lastSetupFunc)
				{
					dc.MaterialSetup(cl, materialInfo);
					lastSetupFunc = dc.MaterialSetup;
				}

				dc.Material->PrepareForRendering(cl);

				cl->SetGraphics32BitConstants(0, TransformCb{ _jitteredCamera.ViewProj * m, m });

				cl->SetVertexBuffer(0, dc.VertexBuffer);
				cl->SetIndexBuffer(dc.IndexBuffer);
				cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
			}
		}

		DxBarrierBatcher(cl)
		.TransitionBegin(_worldNormalsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		.TransitionBegin(_screenVelocitiesTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		DxBarrierBatcher(cl)
		.Transition(_hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		.Transition(_depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ)
		.TransitionEnd(_worldNormalsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		.TransitionEnd(_screenVelocitiesTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		.Transition(_reflectanceTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		.TransitionEnd(FrameResult, frameResultState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.TransitionEnd(_linearDepthBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
#endif

		Ptr<DxTexture> hdrResult = _hdrColorTexture;
		D3D12_RESOURCE_STATES hdrPostProcessingTextureState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		D3D12_RESOURCE_STATES hdrColorTextureState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		cl->CopyResource(_depthStencilBuffer->Resource, _opaqueDepthBuffer->Resource);

		DxBarrierBatcher(cl).TransitionBegin(_opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


		// ----------------------------------------
		// SCREEN SPACE REFLECTIONS
		// ----------------------------------------
#if 1
		if (Settings.EnableSSR) {
			DX_PROFILE_BLOCK(cl, "Screen space reflections");

			{
				DX_PROFILE_BLOCK(cl, "Raycast");

				cl->SetPipelineState(*_ssrRaycastPipeline.Pipeline);
				cl->SetComputeRootSignature(*_ssrRaycastPipeline.RootSignature);

				Settings.SSR.Dimensions = vec2((float)_ssrRaycastTexture->Width, (float)_ssrRaycastTexture->Height);
				Settings.SSR.InvDimensions = vec2(1.f / _ssrRaycastTexture->Width, 1.f / _ssrRaycastTexture->Height);
				Settings.SSR.FrameIndex = (uint32)dxContext.FrameId();

				cl->SetCompute32BitConstants(SsrRaycastRsCb, Settings.SSR);
				cl->SetComputeDynamicConstantBuffer(SsrRaycastRsCamera, materialInfo.CameraCBV);
				cl->SetDescriptorHeapUAV(SsrRaycastRsTextures, 0, _ssrRaycastTexture);
				cl->SetDescriptorHeapSRV(SsrRaycastRsTextures, 1, _depthStencilBuffer);
				cl->SetDescriptorHeapSRV(SsrRaycastRsTextures, 2, _linearDepthBuffer);
				cl->SetDescriptorHeapSRV(SsrRaycastRsTextures, 3, _worldNormalsTexture);
				cl->SetDescriptorHeapSRV(SsrRaycastRsTextures, 4, _reflectanceTexture);
				cl->SetDescriptorHeapSRV(SsrRaycastRsTextures, 5, _noiseTexture);

				cl->Dispatch(bucketize(SSR_RAYCAST_WIDTH, SSR_BLOCK_SIZE), bucketize(SSR_RAYCAST_HEIGHT, SSR_BLOCK_SIZE));

				DxBarrierBatcher(cl)
					.UAV(_ssrRaycastTexture)
					.Transition(_ssrRaycastTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}

			{
				DX_PROFILE_BLOCK(cl, "Resolve");

				cl->SetPipelineState(*_ssrResolvePipeline.Pipeline);
				cl->SetComputeRootSignature(*_ssrResolvePipeline.RootSignature);

				cl->SetCompute32BitConstants(SsrResolveRsCb, SsrResolveCb{ vec2((float)_ssrResolveTexture->Width, (float)_ssrResolveTexture->Height), vec2(1.f / _ssrResolveTexture->Width, 1.f / _ssrResolveTexture->Height) });
				cl->SetComputeDynamicConstantBuffer(SsrResolveRsCamera, materialInfo.CameraCBV);

				cl->SetDescriptorHeapUAV(SsrResolveRsTextures, 0, _ssrResolveTexture);
				cl->SetDescriptorHeapSRV(SsrResolveRsTextures, 1, _depthStencilBuffer);
				cl->SetDescriptorHeapSRV(SsrResolveRsTextures, 2, _worldNormalsTexture);
				cl->SetDescriptorHeapSRV(SsrResolveRsTextures, 3, _reflectanceTexture);
				cl->SetDescriptorHeapSRV(SsrResolveRsTextures, 4, _ssrRaycastTexture);
				cl->SetDescriptorHeapSRV(SsrResolveRsTextures, 5, _prevFrameHDRColorTexture);
				cl->SetDescriptorHeapSRV(SsrResolveRsTextures, 6, _screenVelocitiesTexture);

				cl->Dispatch(bucketize(SSR_RESOLVE_WIDTH, SSR_BLOCK_SIZE), bucketize(SSR_RESOLVE_HEIGHT, SSR_BLOCK_SIZE));

				DxBarrierBatcher(cl)
					.UAV(_ssrResolveTexture)
					.Transition(_ssrResolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.TransitionBegin(_ssrRaycastTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}


			uint32 ssrOutputIndex = 1 - _ssrHistoryIndex;

			{
				DX_PROFILE_BLOCK(cl, "Temporal");

				cl->SetPipelineState(*_ssrTemporalPipline.Pipeline);
				cl->SetComputeRootSignature(*_ssrTemporalPipline.RootSignature);

				cl->SetCompute32BitConstants(SsrTemporalRsCb, SsrTemporalCb{ vec2(1.f / _ssrResolveTexture->Width, 1.f / _ssrResolveTexture->Height) });

				cl->SetDescriptorHeapUAV(SsrTemporalRsTextures, 0, _ssrTemporalTextures[ssrOutputIndex]);
				cl->SetDescriptorHeapSRV(SsrTemporalRsTextures, 1, _ssrResolveTexture);
				cl->SetDescriptorHeapSRV(SsrTemporalRsTextures, 2, _ssrTemporalTextures[_ssrHistoryIndex]);
				cl->SetDescriptorHeapSRV(SsrTemporalRsTextures, 3, _screenVelocitiesTexture);

				cl->Dispatch(bucketize(SSR_RESOLVE_WIDTH, SSR_BLOCK_SIZE), bucketize(SSR_RESOLVE_HEIGHT, SSR_BLOCK_SIZE));

				DxBarrierBatcher(cl)
					.UAV(_ssrTemporalTextures[ssrOutputIndex])
					.Transition(_ssrTemporalTextures[ssrOutputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.TransitionBegin(_ssrTemporalTextures[_ssrHistoryIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.Transition(_ssrResolveTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}

			{
				DX_PROFILE_BLOCK(cl, "Median Blur");

				cl->SetPipelineState(*_ssrMedianBlurPipeline.Pipeline);
				cl->SetComputeRootSignature(*_ssrMedianBlurPipeline.RootSignature);

				cl->SetCompute32BitConstants(SsrMedianBlurRsCb, SsrMedianBlurCb{ vec2(1.f / _ssrResolveTexture->Width, 1.f / _ssrResolveTexture->Height) });

				cl->SetDescriptorHeapUAV(SsrMedianBlurRsTextures, 0, _ssrResolveTexture); // We reuse the resolve texture here.
				cl->SetDescriptorHeapSRV(SsrMedianBlurRsTextures, 1, _ssrTemporalTextures[ssrOutputIndex]);

				cl->Dispatch(bucketize(SSR_RESOLVE_WIDTH, SSR_BLOCK_SIZE), bucketize(SSR_RESOLVE_HEIGHT, SSR_BLOCK_SIZE));

				DxBarrierBatcher(cl)
					.UAV(_ssrResolveTexture)
					.Transition(_ssrResolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}

			{
				DX_PROFILE_BLOCK(cl, "Combine");

				SpecularAmbient(cl, materialInfo.CameraCBV, hdrResult, _ssrResolveTexture, _hdrPostProcessingTexture);
			}

			DxBarrierBatcher(cl)
				.UAV(_hdrPostProcessingTexture)
				.Transition(_hdrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); // Will be read by rest of post processing stack.

		}
		else {
			SpecularAmbient(cl, materialInfo.CameraCBV, hdrResult, 0, _hdrPostProcessingTexture);

			DxBarrierBatcher(cl)
				.UAV(_hdrPostProcessingTexture)
				.Transition(_hdrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); // Will be read by rest of post processing stack.
		}

		hdrResult = _hdrPostProcessingTexture;
		hdrPostProcessingTextureState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		DxBarrierBatcher(cl)
		.TransitionEnd(_opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// After this there is no more camera jittering!
		materialInfo.CameraCBV = unjitteredCameraCBV;
		materialInfo.OpaqueDepth = _opaqueDepthBuffer;
		materialInfo.WorldNormals = _worldNormalsTexture;
#endif
#if 1
		// ----------------------------------------
		// TRANSPARENT LIGHT PASS
		// ----------------------------------------
		if (_transparentRenderPass && _transparentRenderPass->_drawCalls.size() > 0) {
			DX_PROFILE_BLOCK(cl, "Transparent light pass");

			DxBarrierBatcher(cl)
			.Transition(hdrResult, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.Transition(_depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			DxRenderTarget hdrTransparentRenderTarget({ hdrResult }, _depthStencilBuffer);

			cl->SetRenderTarget(hdrTransparentRenderTarget);
			cl->SetViewport(hdrTransparentRenderTarget.Viewport);

			MaterialSetupFunction lastSetupFunc = 0;

			for (const auto& dc : _transparentRenderPass->_drawCalls) {
				const mat4& m = dc.Transform;
				const SubmeshInfo& submesh = dc.Submesh;

				if (dc.MaterialSetup != lastSetupFunc)
				{
					dc.MaterialSetup(cl, materialInfo);
					lastSetupFunc = dc.MaterialSetup;
				}

				dc.Material->PrepareForRendering(cl);

				cl->SetGraphics32BitConstants(0, TransformCb{ _unjitteredCamera.ViewProj * m, m });

				cl->SetVertexBuffer(0, dc.VertexBuffer);
				cl->SetIndexBuffer(dc.IndexBuffer);
				cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
			}


			DxBarrierBatcher(cl)
				.Transition(hdrResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.Transition(_depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		if (Settings.EnableSSR) {
			DxBarrierBatcher(cl)
			.TransitionEnd(_ssrRaycastTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS) // For next frame.
			.TransitionEnd(_ssrTemporalTextures[_ssrHistoryIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS) // For next frame.
			.Transition(_ssrResolveTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.

			_ssrHistoryIndex = 1 - _ssrHistoryIndex;
		}
#endif
		// ----------------------------------------
		// POST PROCESSING
		// ----------------------------------------
		// TAA.
#if 0
		if (Settings.EnableTemporalAntialiasing) {
			DX_PROFILE_BLOCK(cl, "Temporal anti-aliasing");

			uint32 taaOutputIndex = 1 - _taaHistoryIndex;

			cl->SetPipelineState(*_taaPipeline.Pipeline);
			cl->SetComputeRootSignature(*_taaPipeline.RootSignature);

			cl->SetDescriptorHeapUAV(TaaRsTextures, 0, _taaTextures[taaOutputIndex]);
			cl->SetDescriptorHeapSRV(TaaRsTextures, 1, hdrResult);
			cl->SetDescriptorHeapSRV(TaaRsTextures, 2, _taaTextures[_taaHistoryIndex]);
			cl->SetDescriptorHeapSRV(TaaRsTextures, 3, _screenVelocitiesTexture);
			cl->SetDescriptorHeapSRV(TaaRsTextures, 4, _opaqueDepthBuffer);

			cl->SetCompute32BitConstants(TaaRsCb, TaaCb{ _jitteredCamera.ProjectionParams, vec2((float)RenderWidth, (float)RenderHeight) });

			cl->Dispatch(bucketize(RenderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(RenderHeight, POST_PROCESSING_BLOCK_SIZE));

			hdrResult = _taaTextures[taaOutputIndex];

			DxBarrierBatcher(cl)
				.UAV(_taaTextures[taaOutputIndex])
				.Transition(_taaTextures[taaOutputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) // Will be read by rest of post processing stack. Can stay in read state, since it is read as history next frame.
				.Transition(_taaTextures[_taaHistoryIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // Will be used as UAV next frame.

			_taaHistoryIndex = taaOutputIndex;
		}
#endif

		// At this point hdrResult is either the TAA result, the hdrColorTexture, or the hdrPostProcessingTexture. All of these are in read state.

		// Downsample scene. This is also the copy used in SSR next frame.
#if 0
		{
			DX_PROFILE_BLOCK(cl, "Downsample scene");

			DxBarrierBatcher(cl)
				.Transition(_prevFrameHDRColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			cl->SetPipelineState(*_blitPipeline.Pipeline);
			cl->SetComputeRootSignature(*_blitPipeline.RootSignature);

			cl->SetCompute32BitConstants(BlitRsCb, BlitCb{ vec2(1.f / _prevFrameHDRColorTexture->Width, 1.f / _prevFrameHDRColorTexture->Height) });
			cl->SetDescriptorHeapUAV(BlitRsTextures, 0, _prevFrameHDRColorTexture);
			cl->SetDescriptorHeapSRV(BlitRsTextures, 1, hdrResult);

			cl->Dispatch(bucketize(_prevFrameHDRColorTexture->Width, POST_PROCESSING_BLOCK_SIZE), bucketize(_prevFrameHDRColorTexture->Height, POST_PROCESSING_BLOCK_SIZE));

			DxBarrierBatcher(cl)
				.UAV(_prevFrameHDRColorTexture)
				.Transition(_prevFrameHDRColorTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			for (uint32 i = 0; i < _prevFrameHDRColorTexture->NumMipLevels() - 1; ++i) {
				GaussianBlur(cl, _prevFrameHDRColorTexture, _prevFrameHDRColorTempTexture, i, i + 1, EGaussian_Blur_5x5);
			}
		}
#endif
		// Bloom.
#if 0
		if (Settings.EnableBloom) {
			DX_PROFILE_BLOCK(cl, "Bloom");

			{
				DX_PROFILE_BLOCK(cl, "Threshold");

				cl->SetPipelineState(*_bloomThresholdPipline.Pipeline);
				cl->SetComputeRootSignature(*_bloomThresholdPipline.RootSignature);

				cl->SetDescriptorHeapUAV(BloomThresholdRsTextures, 0, _bloomTexture);
				cl->SetDescriptorHeapSRV(BloomThresholdRsTextures, 1, hdrResult);

				cl->SetCompute32BitConstants(BloomThresholdRsCb, BloomThresholdCb{ vec2(1.f / _bloomTexture->Width, 1.f / _bloomTexture->Height), Settings.BloomThreshold });

				cl->Dispatch(bucketize(_bloomTexture->Width, POST_PROCESSING_BLOCK_SIZE), bucketize(_bloomTexture->Height, POST_PROCESSING_BLOCK_SIZE));
			}

			Ptr<DxTexture> bloomResult = (hdrResult == _hdrColorTexture) ? _hdrPostProcessingTexture : _hdrColorTexture;
			D3D12_RESOURCE_STATES& state = (hdrResult == _hdrColorTexture) ? hdrPostProcessingTextureState : hdrColorTextureState;

			{
				DxBarrierBatcher batcher(cl);
				batcher.UAV(_bloomTexture);
				batcher.Transition(_bloomTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

				if (state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
					batcher.Transition(bloomResult, state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				}
			}

			for (uint32 i = 0; i < _bloomTexture->NumMipLevels() - 1; ++i) {
				GaussianBlur(cl, _bloomTexture, _bloomTempTexture, i, i + 1, EGaussian_Blur_9x9);
			}

			{
				DX_PROFILE_BLOCK(cl, "Combine");

				cl->SetPipelineState(*_bloomCombinePipeline.Pipeline);
				cl->SetComputeRootSignature(*_bloomCombinePipeline.RootSignature);

				cl->SetDescriptorHeapUAV(BloomCombineRsTextures, 0, bloomResult);
				cl->SetDescriptorHeapSRV(BloomCombineRsTextures, 1, hdrResult);
				cl->SetDescriptorHeapSRV(BloomCombineRsTextures, 2, _bloomTexture);

				cl->SetCompute32BitConstants(BloomCombineRsCb, BloomCombineCb{ vec2(1.f / RenderWidth, 1.f / RenderHeight), Settings.BloomStrength });

				cl->Dispatch(bucketize(RenderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(RenderHeight, POST_PROCESSING_BLOCK_SIZE));
			}

			hdrResult = bloomResult;

			DxBarrierBatcher(cl)
				.UAV(hdrResult)
				.Transition(bloomResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) // Will be read by rest of post processing stack.
				.Transition(_bloomTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.

			state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
#endif
		// At this point hdrResult is either the TAA result, the hdrColorTexture, or the hdrPostProcessingTexture. All of these are in read state.

		// ----------------------------------------
		// LDR RENDERING
		// ----------------------------------------
		bool renderingOverlays = _overlayRenderPass && _overlayRenderPass->_drawCalls.size();
		bool renderingOutlines = _opaqueRenderPass && _opaqueRenderPass->_outlinedObjects.size() > 0 || _transparentRenderPass && _transparentRenderPass->_outlinedObjects.size() > 0;
		bool renderingToLDRPostprocessingTexture = renderingOverlays || renderingOutlines;

		D3D12_RESOURCE_STATES ldrPostProcessingTextureState = renderingToLDRPostprocessingTexture ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		Tonemap(cl, hdrResult, ldrPostProcessingTextureState);
		cl->TransitionBarrier(_depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		// ----------------------------------------
		// OVERLAYS
		// ----------------------------------------
		DxRenderTarget ldrRenderTarget({ _ldrPostProcessingTexture }, _depthStencilBuffer);
#if 0
		if (renderingOverlays) {
			DX_PROFILE_BLOCK(cl, "3D Overlays");

			cl->SetRenderTarget(ldrRenderTarget);
			cl->SetViewport(ldrRenderTarget.Viewport);

			cl->ClearDepth(_depthStencilBuffer->DSVHandles());

			MaterialSetupFunction lastSetupFunc = 0;

			for (const auto& dc : _overlayRenderPass->_drawCalls) {
				const mat4& m = dc.Transform;

				if (dc.MaterialSetup != lastSetupFunc) {
					dc.MaterialSetup(cl, materialInfo);
					lastSetupFunc = dc.MaterialSetup;
				}

				dc.Material->PrepareForRendering(cl);

				if (dc.SetTransform) {
					cl->SetGraphics32BitConstants(0, TransformCb{ _unjitteredCamera.ViewProj * m, m });
				}

				if (dc.DrawType == GeometryRenderPass::EDrawTypeDefault) {
					const SubmeshInfo& submesh = dc.Submesh;

					cl->SetVertexBuffer(0, dc.VertexBuffer);
					cl->SetIndexBuffer(dc.IndexBuffer);
					cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
				}
				else {
					cl->DispatchMesh(dc.DispatchInfo.DispatchX, dc.DispatchInfo.DispatchY, dc.DispatchInfo.DispatchZ);
				}
			}
		}
#endif
		// ----------------------------------------
		// OUTLINES
		// ----------------------------------------
#if 0
		if (renderingOutlines) {
			DX_PROFILE_BLOCK(cl, "Outlines");

			cl->SetRenderTarget(ldrRenderTarget);
			cl->SetViewport(ldrRenderTarget.Viewport);

			cl->SetStencilReference(EStencilFlagSelectedObject);

			cl->SetPipelineState(*_outlineMarkerPipeline.Pipeline);
			cl->SetGraphicsRootSignature(*_outlineMarkerPipeline.RootSignature);

			// Mark object in stencil.
			auto mark = [](const GeometryRenderPass& rp, DxCommandList* cl, const mat4& viewProj) {
				for (const auto& outlined : rp._outlinedObjects) {
					const SubmeshInfo& submesh = rp._drawCalls[outlined].Submesh;
					const mat4& m = rp._drawCalls[outlined].Transform;
					const auto& vertexBuffer = rp._drawCalls[outlined].VertexBuffer;
					const auto& indexBuffer = rp._drawCalls[outlined].IndexBuffer;

					cl->SetGraphics32BitConstants(OutlineRsMvp, OutlineMarkerCb{ viewProj * m });

					cl->SetVertexBuffer(0, vertexBuffer);
					cl->SetIndexBuffer(indexBuffer);
					cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
				}
			};

			mark(*_opaqueRenderPass, cl, _unjitteredCamera.ViewProj);
			//mark(*transparentRenderPass, cl, unjitteredCamera.viewProj);

			// Draw outline.
			cl->TransitionBarrier(_depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);

			cl->SetPipelineState(*_outlineDrawerPipline.Pipeline);
			cl->SetGraphicsRootSignature(*_outlineDrawerPipline.RootSignature);

			cl->SetGraphics32BitConstants(OutlineRsCb, OutlineDrawerCb{ (int)RenderWidth, (int)RenderHeight });
			cl->SetDescriptorHeapResource(OutlineRsStencil, 0, 1, _depthStencilBuffer->StencilSRV());

			cl->DrawFullscreenTriangle();

			cl->TransitionBarrier(_depthStencilBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}
#endif
		DxBarrierBatcher(cl).Transition(_ldrPostProcessingTexture, ldrPostProcessingTextureState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// TODO: If we really care we should sharpen before rendering overlays and outlines.
		Present(cl);
		DxBarrierBatcher(cl)
		.UAV(FrameResult)
		.Transition(_shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
		.Transition(_hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Transition(_hdrPostProcessingTexture, hdrPostProcessingTextureState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS) // If texture is unused, this results in a NOP.
		.Transition(_worldNormalsTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Transition(_screenVelocitiesTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.Transition(_ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.Transition(FrameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON)
		.Transition(_linearDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.Transition(_opaqueDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
		.Transition(_reflectanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		DxBarrierBatcher(cl).TransitionEnd(_objectIDsTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	else if (dxContext.RaytracingSupported() && _raytracer) {
		DxBarrierBatcher(cl)
			.Transition(_hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.TransitionEnd(FrameResult, frameResultState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		{
			DX_PROFILE_BLOCK(cl, "Raytracing");

			dxContext.RenderQueue.WaitForOtherQueue(dxContext.ComputeQueue); // Wait for AS-rebuilds. TODO: This is not the way to go here. We should wait for the specific value returned by executeCommandList.

			_raytracer->Render(cl, *_tlas, _hdrColorTexture, materialInfo);
		}

		cl->ResetToDynamicDescriptorHeap();

		DxBarrierBatcher(cl)
			.UAV(_hdrColorTexture)
			.Transition(_hdrColorTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		TonemapAndPresent(cl, _hdrColorTexture);

		DxBarrierBatcher(cl)
			.Transition(_hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.Transition(_ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.Transition(FrameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	}

	dxContext.ExecuteCommandList(cl);
}
