#include "DxRenderer.h"
#include "DxContext.h"
#include "DxCommandList.h"
#include "DirectXColors.h"
#include "BarrierBatcher.h"
#include "../physics/geometry.h"
#include "DxTexture.h"
#include "../physics/mesh.h"
#include "../core/random.h"

#include "model_rs.hlsli"
#include "sky_rs.hlsli"

#include <iostream>

DxRenderer* DxRenderer::_instance = new DxRenderer{};

void DxRenderer::Initialize(uint32 width, uint32 height) {
	const aiScene* scene = LoadAssimpScene("assets/meshes/Kettle.fbx");
	AnalyzeScene(scene);
	FreeScene(scene);

	_renderWidth = width;
	_renderHeight = height;

	GlobalDescriptorHeap = DxCbvSrvUavDescriptorHeap::Create(2048);

	FrameResult = DxTexture::Create(nullptr, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, true);
	SetName(FrameResult.Resource, "Frame result");

	DepthBuffer = DxTexture::CreateDepth(width, height, DXGI_FORMAT_D32_FLOAT);
	SetName(DepthBuffer.Resource, "Frame depth buffer");

	HdrRenderTarget.PushColorAttachment(FrameResult);
	HdrRenderTarget.PushDepthStencilAttachment(DepthBuffer);

	_camera.Position = vec3(0.f, 0.f, 4.f);
	_camera.Rotation = quat::identity;
	_camera.VerticalFOV = deg2rad(70.f);
	_camera.NearPlane = 0.1f;

	CpuMesh cpuMesh(EMeshCreationFlagsWithPositions | EMeshCreationFlagsWithUvs | EMeshCreationFlagsWithNormals);
	_submesh = cpuMesh.PushSphere(15, 15, 1.f);
	_mesh = cpuMesh.CreateDxMesh();

	DxPipelineFactory* pipelineFactory = DxPipelineFactory::Instance();

	_hdrColorTexture = DxTexture::LoadFromFile("assets/textures/Material.001_Base_color.png");
	_hdrColorTextureSrv = GlobalDescriptorHeap.Push2DTextureSRV(_hdrColorTexture);

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.RenderTargets(HdrRenderTarget.RenderTargetFormat)
		.DepthSettings(false, false)
		.CullingOff();

		_presentPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "fullscreen_triangle_vs", "present_ps" }, "present_ps");
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.InputLayout(inputLayoutPositionUvNormal)
		.RasterizeCounterClockwise()
		.RenderTargets(HdrRenderTarget.RenderTargetFormat, HdrRenderTarget.DepthStencilFormat);

		_modelPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "model_vs", "model_ps" }, "model_vs");
	}

	pipelineFactory->CreateAllReloadablePipelines();

	memcpy(&_clearColor, DirectX::Colors::LightSteelBlue, sizeof(float) * 4);
}

void DxRenderer::BeginFrame(uint32 width, uint32 height) {
	HdrRenderTarget.ColorAttachments[0] = FrameResult.Resource;
	HdrRenderTarget.RtvHandles[0] = FrameResult.RTVHandles.CpuHandle;

	if (_renderWidth != width or _renderHeight != height) {
		_renderWidth = width;
		_renderHeight = height;

		FrameResult.Resize(width, height);
		DepthBuffer.Resize(width, height);

		HdrRenderTarget.NotifyOnTextureResize(width, height);
	}

	DxPipelineFactory::Instance()->CheckForChangedPipelines();

	_camera.RecalculateMatrices(width, height);
	_viewport = HdrRenderTarget.Viewport;
}

void DxRenderer::BeginFrame(CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, DxResource renderTarget) {
	HdrRenderTarget.ColorAttachments[0] = renderTarget;
	HdrRenderTarget.RtvHandles[0] = rtvHandle;

	uint32 width = renderTarget->GetDesc().Width;
	uint32 height = renderTarget->GetDesc().Height;
	if (width != _renderWidth or height != _renderHeight) {
		_renderHeight = height;
		_renderWidth = width;

		DepthBuffer.Resize(width, height);
	}

	DxPipelineFactory::Instance()->CheckForChangedPipelines();

	_camera.RecalculateMatrices(_renderWidth, _renderHeight);
	_viewport = D3D12_VIEWPORT(0, 0, _renderWidth, _renderHeight);
}


int DxRenderer::DummyRender(float dt) {
	DxContext& dxContext = DxContext::Instance();
	DxCommandList* cl = dxContext.GetFreeRenderCommandList();

	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, _renderWidth, _renderHeight);

	cl->SetScissor(scissorRect);
	cl->SetViewport(_viewport);

	BarrierBatcher(cl).Transition(HdrRenderTarget.ColorAttachments[0], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cl->ClearRTV(HdrRenderTarget.RtvHandles[0], _clearColor);
	cl->ClearDepth(HdrRenderTarget.DsvHandle);
	cl->SetRenderTarget(HdrRenderTarget);

	cl->SetPipelineState(*_presentPipeline.Pipeline);
	cl->SetGraphicsRootSignature(*_presentPipeline.RootSignature);
	cl->SetGraphics32BitConstants(0, _tonemap);
	cl->DrawFullscreenTriangle();

	cl->SetPipelineState(*_modelPipeline.Pipeline);
	cl->SetGraphicsRootSignature(*_modelPipeline.RootSignature);
	cl->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->SetVertexBuffer(0, _mesh.VertexBuffer);
	cl->SetIndexBuffer(_mesh.IndexBuffer);
	cl->SetGraphics32BitConstants(0, TransformCb{ _camera.ViewProj, mat4::identity });
	cl->DrawIndexed(_submesh.NumTriangles * 3, 1, _submesh.FirstTriangle * 3, _submesh.BaseVertex, 0);

	BarrierBatcher(cl).Transition(HdrRenderTarget.ColorAttachments[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	HdrRenderTarget.ColorAttachments[0].Reset();

	return dxContext.ExecuteCommandList(cl);
}

