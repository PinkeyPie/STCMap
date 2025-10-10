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
		auto desc = CREATE_GRAPHICS_PIPELINE.RenderTargets(RenderTarget.RenderTargetFormat).DepthSettings(false, false).CullingOff();

		_presentPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "present_ps", "fullscreen_triangle_vs", "present_ps" });
	}

	{
		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		auto desc = CREATE_GRAPHICS_PIPELINE.InputLayout(inputLayout, std::size(inputLayout)).RasterizeCounterClockwise().RenderTargets(RenderTarget.RenderTargetFormat, RenderTarget.DepthStencilFormat);

		_modelPipeline = pipelineFactory->CreateReloadablePipeline(desc, { "model_vs", "model_vs", "model_ps" });
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
	CD3DX12_RESOURCE_DESC desc(CurrentBackBuffer->GetDesc());
	if (desc.Width != RenderWidth or desc.Height != RenderHeight) {
		RenderHeight = desc.Height;
		RenderWidth = desc.Width;
	}

	DxPipelineFactory::Instance()->CheckForChangedPipelines();

	_camera.RecalculateMatrices(RenderWidth, RenderHeight);
	_viewport = D3D12_VIEWPORT(0, 0, RenderWidth, RenderHeight);
}


int DxRenderer::DummyRender() {
	DxContext& dxContext = DxContext::Instance();
	DxCommandList* cl = dxContext.GetFreeRenderCommandList();

	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

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

	return dxContext.ExecuteCommandList(cl);
}

