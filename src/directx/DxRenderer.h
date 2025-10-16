#pragma once

#include "DxPipeline.h"
#include "DxRenderPrimitives.h"
#include "DxRenderTarget.h"
#include "../core/camera.h"

#include "present_rs.hlsli"

class DxRenderer {
public:
	DxRenderer() = default;

	static DxRenderer* Instance() {
		return _instance;
	}
	void Initialize(uint32 width, uint32 height);

	void BeginFrame(uint32 width, uint32 height);
	void BeginFrame(CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, DxResource renderTarget);
	
	int DummyRender(float dt);

	DxCbvSrvUavDescriptorHeap GlobalDescriptorHeap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv;
	DxTexture FrameResult;
	DxTexture DepthBuffer;

	DxRenderTarget RenderTarget;

	uint32 RenderWidth;
	uint32 RenderHeight;

private:
	static DxRenderer* _instance;

	RenderCamera _camera = {};
	DxMesh _mesh = {};
	SubmeshInfo _submesh = {};

	TonemapCb _tonemap = DefaultTonemapParameters();

	DxPipeline _presentPipeline;
	DxPipeline _modelPipeline;
	D3D12_VIEWPORT _viewport;

	DxTexture _texture;
	DxDescriptorHandle _textureHandle;

	float _clearColor[4];
};