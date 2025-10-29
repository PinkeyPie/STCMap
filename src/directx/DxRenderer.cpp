#include "DxRenderer.h"
#include "DxContext.h"
#include "DxCommandList.h"
#include "DirectXColors.h"
#include "BarrierBatcher.h"
#include "../physics/geometry.h"
#include "DxTexture.h"
#include "../physics/mesh.h"
#include "../core/random.h"
#include "../render/TexturePreprocessing.h"

#include "model_rs.hlsli"
#include "outline_rs.hlsli"
#include "sky_rs.hlsli"

#include <iostream>

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

void DxRenderer::Initialize(uint32 width, uint32 height) {
	_windowWidth = width;
	_windowHeight = height;

	RecalculateViewport(false);

	GlobalDescriptorHeap = DxCbvSrvUavDescriptorHeap::Create(2048);

	uint8 white[] = { 255, 255, 255, 255 };
	_whiteTexture = DxTexture::Create(white, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	_whiteTextureSRV = GlobalDescriptorHeap.Push2DTextureSRV(_whiteTexture);

	TexturePreprocessor* preprocessor = TexturePreprocessor::Instance();
	preprocessor->Initialize();

	// Frame result
	{
		FrameResult = DxTexture::Create(nullptr, _windowWidth, _windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true);
		SetName(FrameResult.Resource, "Frame result");

		_windowRenderTarget.PushColorAttachment(FrameResult);
	}

	// HDR render target
	{
		_depthBuffer = DxTexture::CreateDepth(_renderWidth, _renderHeight, DXGI_FORMAT_D32_FLOAT);
		SetName(_depthBuffer.Resource, "HDR depth buffer");

		_hdrColorTexture = DxTexture::Create(nullptr, _renderWidth, _renderHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
		SetName(_hdrColorTexture.Resource, "HDR Color texture");
		_hdrColorTextureSrv = GlobalDescriptorHeap.Push2DTextureSRV(_hdrColorTexture);

		_hdrRenderTarget.PushColorAttachment(_hdrColorTexture);
		_hdrRenderTarget.PushDepthStencilAttachment(_depthBuffer);
	}

	DxPipelineFactory* pipelineFactory = DxPipelineFactory::Instance();

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.RenderTargets(_windowRenderTarget.RenderTargetFormat)
		.DepthSettings(false, false);

		_presentPipeline = pipelineFactory->CreateReloadablePipeline(desc, {"fullscreen_triangle_vs", "present_ps"});
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.InputLayout(inputLayoutPositionUvNormalTangent)
		.RenderTargets(0, 0, _hdrRenderTarget.DepthStencilFormat);

		_modelDepthOnlyPipeline = pipelineFactory->CreateReloadablePipeline(desc, {"model_vs"}, "model_vs"); // The depth only RS is baked into the vertex shader

		desc.RenderTargets(_hdrRenderTarget.RenderTargetFormat, _hdrRenderTarget.DepthStencilFormat)
		.StencilSettings(D3D12_COMPARISON_FUNC_ALWAYS, D3D12_STENCIL_OP_REPLACE, D3D12_STENCIL_OP_REPLACE) // Mark areas in stencil, for example for outline
		.DepthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL);

		_modelPipeline = pipelineFactory->CreateReloadablePipeline(desc, {"model_vs", "model_ps"});
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.InputLayout(inputLayoutPosition)
		.RenderTargets(_hdrRenderTarget.RenderTargetFormat)
		.DepthSettings(false, false)
		.CullFrontFaces();

		_proceduralSkyPipeline = pipelineFactory->CreateReloadablePipeline(desc, {"sky_vs", "sky_procedural_ps"});
		_textureSkyPipeline = pipelineFactory->CreateReloadablePipeline(desc, {"sky_vs", "sky_texture_ps"});
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.InputLayout(inputLayoutPositionUvNormalTangent)
		.RenderTargets(_hdrRenderTarget.RenderTargetFormat, _hdrRenderTarget.DepthStencilFormat)
		.StencilSettings(D3D12_COMPARISON_FUNC_NOT_EQUAL);

		_outlinePipeline = pipelineFactory->CreateReloadablePipeline(desc, {"outline_vs", "outline_ps"}, "outline_vs");
	}

	pipelineFactory->CreateAllReloadablePipelines();

	_camera.Position = vec3(0.f,0.f,4.f);
	_camera.Rotation = quat::identity;
	_camera.VerticalFOV = deg2rad(70.f);
	_camera.NearPlane = 0.1f;

	{
		CpuMesh mesh(EMeshCreationFlagsWithPositions | EMeshCreationFlagsWithUvs | EMeshCreationFlagsWithNormals | EMeshCreationFlagsWithTangents);
		float shaftLength = 2.f;
		float headLength = 0.4f;
		float radius = 0.06f;
		float headRadius = 0.13f;
		TranslationGizmoSubmesh = mesh.PushArrow(6, radius, headRadius, shaftLength, headLength);
		RotationGizmoSubmesh = mesh.PushTorus(6, 64, shaftLength, radius);
		ScaleGizmoSubmesh = mesh.PushMace(6, radius, headRadius, shaftLength, headLength);
		_gizmoMesh = mesh.CreateDxMesh();
	}

	{
		CpuMesh _mesh(EMeshCreationFlagsWithPositions);
		_mesh.PushCube(1.f);
		_skyMesh = _mesh.CreateDxMesh();
	}

	DxContext& dxContext = DxContext::Instance();
	{
		DxCommandList* list = dxContext.GetFreeRenderCommandList();
		_brdfTex = preprocessor->IntegrateBRDF(list);
		_brdfTexSRV = GlobalDescriptorHeap.Push2DTextureSRV(_brdfTex);
		dxContext.ExecuteCommandList(list);
	}

	_sceneMesh = CreateCompositeMeshFromFile("assets/meshes/Kettle.fbx", EMeshCreationFlagsWithPositions | EMeshCreationFlagsWithUvs | EMeshCreationFlagsWithNormals | EMeshCreationFlagsWithTangents);

	_meshAlbedoTex = DxTexture::LoadFromFile("assets/textures/cerebrus_a.tga");
	_meshNormalTex = DxTexture::LoadFromFile("assets/textures/cerebrus_n.tga", ETextureLoadFlagsDefault | ETextureLoadFlagsNoncolor);
	_meshRoughTex = DxTexture::LoadFromFile("assets/textures/cerebrus_r.tga", ETextureLoadFlagsDefault | ETextureLoadFlagsNoncolor);
	_meshMetalTex = DxTexture::LoadFromFile("assets/textures/cerebrus_m.tga", ETextureLoadFlagsDefault | ETextureLoadFlagsNoncolor);
	_textureSRV = GlobalDescriptorHeap.Push2DTextureSRV(_meshAlbedoTex);
	GlobalDescriptorHeap.Push2DTextureSRV(_meshNormalTex);
	GlobalDescriptorHeap.Push2DTextureSRV(_meshRoughTex);
	GlobalDescriptorHeap.Push2DTextureSRV(_meshMetalTex);

	_meshTransform = trs::identity;
	_meshTransform.rotation = quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f));
	_meshTransform.scale = 0.04f;

	{
		DxTexture sky = DxTexture::LoadFromFile("assets/textures/aircraft_workshop_01_4k.hdr",
		ETextureLoadFlagsNoncolor | ETextureLoadFlagsCacheToDds | ETextureLoadFlagsAllocateFullMipChain);

		dxContext.RenderQueue.WaitForOtherQueue(dxContext.CopyQueue);
		DxCommandList* cl = dxContext.GetFreeRenderCommandList();
		preprocessor->GenerateMipMapsOnGPU(cl, sky);
		_environment.Sky = preprocessor->EquirectangularToCubemap(cl, sky, 2048, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		_environment.Prefiltered = preprocessor->PrefilterEnvironment(cl, _environment.Sky, 128);
		_environment.Irradiance = preprocessor->CubemapToIrradiance(cl, _environment.Sky);
		dxContext.ExecuteCommandList(cl);

		_environment.SkySRV = GlobalDescriptorHeap.PushCubemapSRV(_environment.Sky);
		_environment.PrefilteredSRV = GlobalDescriptorHeap.PushCubemapSRV(_environment.Prefiltered);
		_environment.IrradianceSRV = GlobalDescriptorHeap.PushCubemapSRV(_environment.Irradiance);

		dxContext.RetireObject(sky.Resource);
	}

	memcpy(&_clearColor, DirectX::Colors::LightSteelBlue, sizeof(float) * 4);
}

void DxRenderer::BeginFrame(uint32 width, uint32 height) {
	_hdrRenderTarget.ColorAttachments[0] = FrameResult.Resource;
	_hdrRenderTarget.RtvHandles[0] = FrameResult.RTVHandles.CpuHandle;

	if (_windowWidth != width or _windowHeight != height) {
		_windowWidth = width;
		_windowHeight = height;

		// Frame result
		{
			FrameResult.Resize(_windowWidth, _windowHeight);
			_windowRenderTarget.NotifyOnTextureResize(_windowWidth, _windowHeight);
		}

		RecalculateViewport(true);
	}

	DxPipelineFactory::Instance()->CheckForChangedPipelines();

	_camera.RecalculateMatrices(width, height);
}

void DxRenderer::BeginFrame(CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, DxResource renderTarget) {
	_windowRenderTarget.ColorAttachments[0] = renderTarget;
	_windowRenderTarget.RtvHandles[0] = rtvHandle;

	uint32 width = renderTarget->GetDesc().Width;
	uint32 height = renderTarget->GetDesc().Height;
	if (width != _windowWidth or height != _windowHeight) {
		_windowHeight = height;
		_windowWidth = width;

		RecalculateViewport(true);
	}

	DxPipelineFactory::Instance()->CheckForChangedPipelines();

	_camera.RecalculateMatrices(_renderWidth, _renderHeight);
}

void DxRenderer::RecalculateViewport(bool resizeTextures) {
	if (_aspectRatioMode == EAspectRatioFree) {
		_windowViewport = { 0.f, 0.f, (float)_windowWidth, (float)_windowHeight, 0.f, 1.f};
	}
	else {
		const float targetAspect = _aspectRatioMode == EAspectRatioFix16_9 ? (16.f / 9.f) : (16.f/ 10.f);

		float aspect = (float)_windowWidth / (float)_windowHeight;
		if (aspect > targetAspect) {
			float width = _windowHeight * targetAspect;
			float widthOffset = (_windowWidth - width) * 0.5f;
			_windowViewport = { widthOffset, 0.f, width, (float)_windowHeight, 0.f, 1.f};
		}
		else {
			float height = _windowWidth / targetAspect;
			float heightOffset = (_windowHeight - height) * 0.5f;
			_windowViewport = { 0.f, heightOffset, (float)_windowWidth, height, 0.f, 1.f};
		}
	}

	_renderWidth = (uint32)_windowViewport.Width;
	_renderHeight = (uint32)_windowViewport.Height;

	if (resizeTextures) {
		_hdrColorTexture.Resize(_renderWidth, _renderHeight);
		_depthBuffer.Resize(_renderWidth, _renderHeight);
		GlobalDescriptorHeap.Create2DTextureSRV(_hdrColorTexture, _hdrColorTextureSrv);
		_hdrRenderTarget.NotifyOnTextureResize(_renderWidth, _renderHeight);
	}
}

int DxRenderer::DummyRender(float dt) {
	DxContext& dxContext = DxContext::Instance();
	DxCommandList* cl = dxContext.GetFreeRenderCommandList();

	static float meshRotationSpeed = 1.f;
	static GizmoType gizmoType;
	static bool showOutline = false;
	static vec4 outlineColor(1.f, 1.f, 0.f, 1.f);

	DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
	ThrowIfFailed(dxContext.GetAdapter()->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &memoryInfo));

	quat deltaRotation(vec3(0.f, 1.f, 0.f), 2.f * PI * 0.1f * meshRotationSpeed * dt);
	_meshTransform.rotation = deltaRotation * _meshTransform.rotation;

	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

	cl->SetScissor(scissorRect);

	BarrierBatcher(cl)
	.Transition(_hdrColorTexture.Resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cl->SetDescriptorHeap(GlobalDescriptorHeap);

	cl->SetRenderTarget(_hdrRenderTarget);
	cl->SetViewport(_hdrRenderTarget.Viewport);
	cl->ClearDepthAndStencil(_hdrRenderTarget.DsvHandle);

	// Sky
	cl->SetPipelineState(*_textureSkyPipeline.Pipeline);
	cl->SetGraphicsRootSignature(*_textureSkyPipeline.RootSignature);
	cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->SetGraphics32BitConstants(SkyRsVp, SkyCb{_camera.Proj * CreateSkyViewMatrix(_camera.View)});
	cl->SetGraphicsDescriptorTable(SkyRsTex, _environment.SkySRV);

	cl->SetVertexBuffer(0, _skyMesh.VertexBuffer);
	cl->SetIndexBuffer(_skyMesh.IndexBuffer);
	cl->DrawIndexed(_skyMesh.IndexBuffer.ElementCount, 1, 0, 0, 0);

	// Models
	auto submesh = _sceneMesh.SingleMeshes[0].Submesh;
	mat4 m = trsToMat4(_meshTransform);
	cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->SetVertexBuffer(0, _sceneMesh.Mesh.VertexBuffer);
	cl->SetIndexBuffer(_sceneMesh.Mesh.IndexBuffer);

	// Depth-only pass
	cl->SetPipelineState(*_modelDepthOnlyPipeline.Pipeline);
	cl->SetGraphicsRootSignature(*_modelDepthOnlyPipeline.RootSignature);
	cl->SetGraphics32BitConstants(ModelRsMeshViewProj, TransformCb{_camera.ViewProj * m, m});
	cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);

	// Light pass
	cl->SetPipelineState(*_modelPipeline.Pipeline);
	cl->SetGraphicsRootSignature((*_modelPipeline.RootSignature));
	cl->SetGraphicsDescriptorTable(ModelRsPbrTextures, _textureSRV);
	cl->SetGraphicsDescriptorTable(ModelRsEnvironmentTextures, _environment.IrradianceSRV);
	cl->SetGraphicsDescriptorTable(ModelRsBrdf, _brdfTexSRV);
	cl->SetGraphics32BitConstants(ModelRsMaterial, PbrMaterialCb{vec4(1.f, 1.f, 1.f, 1.f), 0.f, 0.f, USE_ALBEDO_TEXTURE | USE_NORMAL_TEXTURE | USE_ROUGHNESS_TEXTURE | USE_METALLIC_TEXTURE});

	cl->SetGraphics32BitConstants(ModelRsMeshViewProj, TransformCb{_camera.ViewProj * m, m});

	cl->SetStencilReference(1);
	cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);

	// Gizmos.
	cl->SetVertexBuffer(0, _gizmoMesh.VertexBuffer);
	cl->SetIndexBuffer(_gizmoMesh.IndexBuffer);

	for (uint32 i = 0; i < 3; i++) {
		mat4 m = CreateModelMatrix(_meshTransform.position, _gizmoRotations[i]);
		cl->SetGraphics32BitConstants(ModelRsMeshViewProj, TransformCb{_camera.ViewProj * m, m});
		cl->SetGraphics32BitConstants(ModelRsMaterial, PbrMaterialCb{_gizmoColors[i], 1.f, 0.f, 0});
		cl->DrawIndexed(gizmoSubmeshes[gizmoType].NumTriangles * 3, 1, gizmoSubmeshes[gizmoType].FirstTriangle * 3, gizmoSubmeshes[gizmoType].BaseVertex, 0);
	}

	// Outline
	if (showOutline) {
		cl->SetPipelineState(*_outlinePipeline.Pipeline);
		cl->SetGraphicsRootSignature(*_outlinePipeline.RootSignature);
		cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cl->SetVertexBuffer(0, _sceneMesh.Mesh.VertexBuffer);
		cl->SetIndexBuffer(_sceneMesh.Mesh.IndexBuffer);

		cl->SetGraphics32BitConstants(OutlineRsMvp, OutlineCb{_camera.ViewProj * m, outlineColor});

		cl->SetStencilReference(1);
		cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
	}

	// Present
	BarrierBatcher(cl)
	.Transition(_hdrColorTexture.Resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	.Transition(_windowRenderTarget.ColorAttachments[0], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cl->SetRenderTarget(_windowRenderTarget);
	cl->SetViewport(_windowViewport);

	cl->SetPipelineState(*_presentPipeline.Pipeline);
	cl->SetGraphicsRootSignature(*_presentPipeline.RootSignature);

	cl->SetGraphics32BitConstants(PresentRsTonemap, _tonemap);
	cl->SetGraphics32BitConstants(PresentRsPresent, PresentCb{0, 0.f});
	cl->SetGraphicsDescriptorTable(PresentRsTex, _hdrColorTextureSrv);

	BarrierBatcher(cl)
	.Transition(_hdrColorTexture.Resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
	.Transition(_windowRenderTarget.ColorAttachments[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);

	_windowRenderTarget.ColorAttachments[0].Reset();
	return dxContext.ExecuteCommandList(cl);
}

