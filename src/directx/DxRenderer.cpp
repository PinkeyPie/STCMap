#include "DxRenderer.h"
#include "DxContext.h"
#include "DxCommandList.h"
#include "DirectXColors.h"

#include "model_rs.hlsli"

#include <iostream>

#include "BarrierBatcher.h"
#include "../physics/geometry.h"

DxRenderer* DxRenderer::_instance = new DxRenderer{};

void DxRenderer::Initialize(uint32 width, uint32 height) {
	RenderWidth = width;
	RenderHeight = height;

	GlobalDescriptorHeap = DxCbvSrvUavDescriptorHeap::Create(2048);

	FrameResult = DxTexture::Create(nullptr, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, true);
	SetName(FrameResult.Resource, "Frame result");

	DepthBuffer = DxTexture::CreateDepth(width, height, DXGI_FORMAT_D32_FLOAT);
	SetName(DepthBuffer.Resource, "Frame depth buffer");

	RenderTarget.PushColorAttachment(FrameResult);
	RenderTarget.PushDepthStencilAttachment(DepthBuffer);

	_camera.Position = vec3(0.f, 0.f, 4.f);
	_camera.Rotation = quat::identity;
	_camera.VerticalFOV = deg2rad(70.f);
	_camera.NearPlane = 0.1f;

	CpuMesh cpuMesh(EMeshCreationFlagsWithPositions | EMeshCreationFlagsWithUvs | EMeshCreationFlagsWithNormals);
	_submesh = cpuMesh.PushSphere(15, 15, 1.f);
	_mesh = cpuMesh.CreateDxMesh();

	DxPipelineFactory* pipelineFactory = DxPipelineFactory::Instance();

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.RenderTargets(RenderTarget.RenderTargetFormat)
		.DepthSettings(false, false)
		.CullingOff();

		_presentPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "fullscreen_triangle_vs", "present_ps" }, "present_ps");
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
		.InputLayout(inputLayoutPositionUvNormal)
		.RasterizeCounterClockwise()
		.RenderTargets(RenderTarget.RenderTargetFormat, RenderTarget.DepthStencilFormat);

		_modelPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "model_vs", "model_ps" }, "model_vs");
	}

	pipelineFactory->CreateAllReloadablePipelines();

	memcpy(&_clearColor, DirectX::Colors::LightSteelBlue, sizeof(float) * 4);
}

void DxRenderer::BeginFrame(uint32 width, uint32 height) {
	if (RenderWidth != width or RenderHeight != height) {
		RenderWidth = width;
		RenderHeight = height;

		FrameResult.Resize(width, height);
		DepthBuffer.Resize(width, height);

		RenderTarget.NotifyOnTextureResize(width, height);
	}

	DxPipelineFactory::Instance()->CheckForChangedPipelines();

	_camera.RecalculateMatrices(width, height);
	_viewport = RenderTarget.Viewport;
	CurrentBackBuffer = FrameResult.Resource;
	BackBufferRtv = RenderTarget.RtvHandles[0];
}

void DxRenderer::BeginFrame(CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, DxResource renderTarget) {
	CurrentBackBuffer = renderTarget;
	BackBufferRtv = rtvHandle;
	uint32 width = renderTarget->GetDesc().Width;
	uint32 height = renderTarget->GetDesc().Height;
	if (width != RenderWidth or height != RenderHeight) {
		RenderHeight = height;
		RenderWidth = width;

		DepthBuffer.Resize(width, height);
	}

	DxPipelineFactory::Instance()->CheckForChangedPipelines();

	_camera.RecalculateMatrices(RenderWidth, RenderHeight);
	_viewport = D3D12_VIEWPORT(0, 0, RenderWidth, RenderHeight);
}


int DxRenderer::DummyRender() {
	DxContext& dxContext = DxContext::Instance();
	DxCommandList* cl = dxContext.GetFreeRenderCommandList();

	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, RenderWidth, RenderHeight);

	cl->SetScissor(scissorRect);
	cl->SetViewport(_viewport);

	BarrierBatcher(cl).Transition(CurrentBackBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cl->ClearRTV(BackBufferRtv, _clearColor);
	cl->ClearDepth(RenderTarget.DsvHandle);
	cl->CommandList->OMSetRenderTargets(1, &BackBufferRtv, false, &RenderTarget.DsvHandle);

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

	BarrierBatcher(cl).Transition(CurrentBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	CurrentBackBuffer.Reset();

	return dxContext.ExecuteCommandList(cl);
}

