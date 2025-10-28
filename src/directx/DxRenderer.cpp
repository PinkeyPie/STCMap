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
		.InputLayout(inputLayoutPositionUvNormal)
		.RasterizeCounterClockwise()
		// .RenderTargets(_hdrRenderTarget.RenderTargetFormat, _hdrRenderTarget.DepthStencilFormat);
		.RenderTargets(_windowRenderTarget.RenderTargetFormat, _hdrRenderTarget.DepthStencilFormat);

		_modelPipeline = pipelineFactory->CreateReloadablePipeline(desc, {"model_vs", "model_ps"});
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.InputLayout(inputLayoutPosition)
		// .RenderTargets(_hdrRenderTarget.RenderTargetFormat)
		.RenderTargets(_windowRenderTarget.RenderTargetFormat)
		.DepthSettings(false, false);

		_proceduralSkyPipeline = pipelineFactory->CreateReloadablePipeline(desc, {"sky_vs", "sky_procedural_ps"});
		_textureSkyPipeline = pipelineFactory->CreateReloadablePipeline(desc, {"sky_vs", "sky_texture_ps"});
	}

	pipelineFactory->CreateAllReloadablePipelines();

	_camera.Position = vec3(0.f,0.f,4.f);
	_camera.Rotation = quat::identity;
	_camera.VerticalFOV = deg2rad(70.f);
	_camera.NearPlane = 0.1f;

	{
		const aiScene* scene = LoadAssimpScene("assets/meshes/Kettle.fbx");
		_sceneMesh = CreateCompositeMeshFromScene(scene);
		FreeScene(scene);
	}

	{
		CpuMesh _mesh(EMeshCreationFlagsWithPositions);
		_mesh.PushCube(1.f);
		_skyMesh = _mesh.CreateDxMesh();
	}

	RandomNumberGenerator rng = { 6718923 };

	_meshTransforms = new trs[_numMeshes];
	_meshModelMatrices = new mat4[_sceneMesh.SingleMeshes.size()];
	for (uint32 i = 0; i < _sceneMesh.SingleMeshes.size(); i++) {
		_meshTransforms[i].position.x = rng.RandomFloatBetween(-30.f, 30.f);
		_meshTransforms[i].position.y = rng.RandomFloatBetween(-30.f, 30.f);
		_meshTransforms[i].position.z = rng.RandomFloatBetween(-30.f, 30.f);

		vec3 rotationAxis = normalize(vec3(rng.RandomFloatBetween(-1.f, 1.f), rng.RandomFloatBetween(-1.f, 1.f), rng.RandomFloatBetween(-1.f, 1.f)));
		_meshTransforms[i].rotation = quat(rotationAxis, rng.RandomFloatBetween(0.f, 2.f * PI));

		_meshTransforms[i].scale = rng.RandomFloatBetween(0.2f, 3.f);
	}

	_texture = DxTexture::LoadFromFile("assets/textures/Material.001_Base_Color.png");
	_textureHandle = GlobalDescriptorHeap.Push2DTextureSRV(_texture);

	DxContext& dxContext = DxContext::Instance();
	{
		// DxCommandList* cl = dxContext.GetFreeRenderCommandList();
		// dxContext.RenderQueue.WaitForOtherQueue(dxContext.CopyQueue);
		DxTexture sky = DxTexture::LoadFromFile("assets/textures/grasscube1024.dds",
		ETextureLoadFlagsNoncolor | ETextureLoadFlagsCacheToDds | ETextureLoadFlagsAllocateFullMipChain);
		// preprocessor->GenerateMipMapsOnGPU(cl, sky);
		// _environment.Sky = preprocessor->EquirectangularToCubemap(cl, sky, 2048, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		_environment.Sky = sky;
		// _environment.Prefiltered = preprocessor->PrefilterEnvironment(cl, _environment.Sky, 128);
		// _environment.Irradiance = preprocessor->CubemapToIrradiance(cl, _environment.Sky);
		// dxContext.ExecuteCommandList(cl);

		_environment.SkyHandle = GlobalDescriptorHeap.PushCubemapSRV(_environment.Sky);
		// _environment.PrefilteredHandle = GlobalDescriptorHeap.PushCubemapSRV(_environment.Prefiltered);
		// _environment.IrradianceHandle = GlobalDescriptorHeap.PushCubemapSRV(_environment.Irradiance);

		// dxContext.RetireObject(sky.Resource);
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

	static uint32 meshLOD = 0;
	static float meshSpeed = 0.f;

	quat meshDeltaRotation(vec3(0.f, 1.f, 0.f), 2.f * PI * 0.1f * meshSpeed * dt);
	for (uint32 i = 0; i < _numMeshes; i++) {
		vec3 position = _meshTransforms[i].position;
		position = meshDeltaRotation * position;

		_meshTransforms[i].rotation = meshDeltaRotation * _meshTransforms[i].rotation;
		_meshTransforms[i].position = position;

		_meshModelMatrices[i] = trsToMat4(_meshTransforms[i]);
	}

	BarrierBatcher(cl)
	.Transition(_windowRenderTarget.ColorAttachments[0], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	cl->SetScissor(scissorRect);

	cl->SetDescriptorHeap(GlobalDescriptorHeap);

	cl->CommandList->OMSetRenderTargets(1, &_windowRenderTarget.RtvHandles[0], FALSE, &_hdrRenderTarget.DsvHandle);
	cl->ClearRTV(_windowRenderTarget.RtvHandles[0], _clearColor);
	cl->SetViewport(_windowRenderTarget.Viewport);
	cl->ClearDepth(_hdrRenderTarget.DsvHandle);

	cl->SetPipelineState(*_textureSkyPipeline.Pipeline);
	cl->SetGraphicsRootSignature(*_textureSkyPipeline.RootSignature);
	cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->SetGraphics32BitConstants(SKY_RS_VP, SkyCb{_camera.Proj * CreateSkyViewMatrix(_camera.View)});
	cl->SetGraphicsDescriptorTable(SKY_RS_TEX, _environment.SkyHandle);

	cl->SetVertexBuffer(0, _skyMesh.VertexBuffer);
	cl->SetIndexBuffer(_skyMesh.IndexBuffer);
	cl->DrawIndexed(_skyMesh.IndexBuffer.ElementCount, 1, 0, 0, 0);

	// Models
	cl->SetPipelineState(*_modelPipeline.Pipeline);
	cl->SetGraphicsRootSignature((*_modelPipeline.RootSignature));
	cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->SetVertexBuffer(0, _sceneMesh.Mesh.VertexBuffer);
	cl->SetIndexBuffer(_sceneMesh.Mesh.IndexBuffer);
	cl->SetGraphicsDescriptorTable(MODEL_RS_ALBEDO, _textureHandle);

	for (uint32 i = 0; i < _numMeshes; i++) {
		mat4& m = _meshModelMatrices[i];
		cl->SetGraphics32BitConstants(MODEL_RS_MVP, TransformCb{_camera.ViewProj * m, m});
		for (auto & singleMesh : _sceneMesh.SingleMeshes) {
			auto submesh = singleMesh.Submesh;
			cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
		}
	}

	BarrierBatcher(cl)
	.Transition(_windowRenderTarget.ColorAttachments[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	_windowRenderTarget.ColorAttachments[0].Reset();

	return dxContext.ExecuteCommandList(cl);
}


// int DxRenderer::DummyRender(float dt) {
// 	DxContext& dxContext = DxContext::Instance();
// 	DxCommandList* cl = dxContext.GetFreeRenderCommandList();
//
// 	static uint32 meshLOD = 0;
// 	static float meshSpeed = 1.f;
//
// 	DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
// 	ThrowIfFailed(dxContext.GetAdapter()->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &memoryInfo));
//
// 	quat meshDeltaRotation(vec3(0.f, 1.f, 0.f), 2.f * PI * 0.1f * meshSpeed * dt);
// 	for (uint32 i = 0; i < _numMeshes; i++) {
// 		vec3 position = _meshTransforms[i].position;
// 		position = meshDeltaRotation * position;
//
// 		_meshTransforms[i].rotation = meshDeltaRotation * _meshTransforms[i].rotation;
// 		_meshTransforms[i].position = position;
//
// 		_meshModelMatrices[i] = trsToMat4(_meshTransforms[i]);
// 	}
//
// 	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
//
// 	cl->SetScissor(scissorRect);
//
// 	BarrierBatcher(cl)
// 	.Transition(_hdrColorTexture.Resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
//
// 	cl->SetDescriptorHeap(GlobalDescriptorHeap);
//
// 	cl->SetRenderTarget(_hdrRenderTarget);
// 	cl->SetViewport(_hdrRenderTarget.Viewport);
// 	cl->ClearDepth(_hdrRenderTarget.DsvHandle);
//
// 	// Sky
// 	cl->SetPipelineState(*_textureSkyPipeline.Pipeline);
// 	cl->SetGraphicsRootSignature(*_textureSkyPipeline.RootSignature);
// 	cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
// 	cl->SetGraphics32BitConstants(SKY_RS_VP, SkyCb{_camera.Proj * CreateSkyViewMatrix(_camera.View)});
// 	cl->SetGraphicsDescriptorTable(SKY_RS_TEX, _environment.SkyHandle);
//
// 	cl->SetVertexBuffer(0, _skyMesh.VertexBuffer);
// 	cl->SetIndexBuffer(_skyMesh.IndexBuffer);
// 	cl->DrawIndexed(_skyMesh.IndexBuffer.ElementCount, 1, 0, 0, 0);
//
// 	// Models
// 	cl->SetPipelineState(*_modelPipeline.Pipeline);
// 	cl->SetGraphicsRootSignature((*_modelPipeline.RootSignature));
// 	cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
// 	cl->SetVertexBuffer(0, _sceneMesh.Mesh.VertexBuffer);
// 	cl->SetIndexBuffer(_sceneMesh.Mesh.IndexBuffer);
// 	cl->SetGraphicsDescriptorTable(MODEL_RS_ALBEDO, _textureHandle);
//
// 	for (uint32 i = 0; i < _numMeshes; i++) {
// 		mat4& m = _meshModelMatrices[i];
// 		cl->SetGraphics32BitConstants(MODEL_RS_MVP, TransformCb{_camera.ViewProj * m, m});
// 		auto submesh = _sceneMesh.SingleMeshes[meshLOD].Submesh;
// 		cl->DrawIndexed(submesh.NumTriangles * 3, 1, submesh.FirstTriangle * 3, submesh.BaseVertex, 0);
// 	}
//
// 	// Present
// 	BarrierBatcher(cl)
// 	.Transition(_hdrColorTexture.Resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
// 	.Transition(_windowRenderTarget.ColorAttachments[0], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
//
// 	cl->SetRenderTarget(_windowRenderTarget);
// 	cl->SetViewport(_windowViewport);
//
// 	cl->SetPipelineState(*_presentPipeline.Pipeline);
// 	cl->SetGraphicsRootSignature(*_presentPipeline.RootSignature);
//
// 	cl->SetGraphics32BitConstants(PRESENT_RS_TONEMAP, _tonemap);
// 	cl->SetGraphics32BitConstants(PRESENT_RS_PRESENT, PresentCb{0, 0.f});
// 	cl->SetGraphicsDescriptorTable(PRESENT_RS_TEX, _hdrColorTextureSrv);
//
// 	BarrierBatcher(cl)
// 	.Transition(_hdrColorTexture.Resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
// 	.Transition(_windowRenderTarget.ColorAttachments[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
//
// 	_windowRenderTarget.ColorAttachments[0].Reset();
// 	return dxContext.ExecuteCommandList(cl);
// }

