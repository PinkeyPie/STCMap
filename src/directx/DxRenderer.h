#pragma once

#include "DxPipeline.h"
#include "DxRenderPrimitives.h"
#include "DxRenderTarget.h"
#include "../core/camera.h"

#include "math.h"
#include "present_rs.hlsli"

enum AspectRatioMode {
	EAspectRatioFree,
	EAspectRatioFix16_9,
	EAspectRatioFix16_10,

	EAspectRatioModeCount
};

static const char* aspectRatioNames[] = {
	"Free",
	"16:9",
	"16:10"
};

struct PbrEnvironment {
	DxTexture Sky;
	DxTexture Prefiltered;
	DxTexture Irradiance;

	DxDescriptorHandle SkyHandle;
	DxDescriptorHandle PrefilteredHandle;
	DxDescriptorHandle IrradianceHandle;
};

class DxRenderer {
public:
	DxRenderer() = default;

	static DxRenderer* Instance() {
		return _instance;
	}
	void Initialize(uint32 width, uint32 height);

	void BeginFrame(uint32 width, uint32 height);
	void BeginFrame(CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, DxResource renderTarget);
	void RecalculateViewport(bool resizeTextures);
	int DummyRender(float dt);

	DxCbvSrvUavDescriptorHeap GlobalDescriptorHeap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv;
	DxTexture FrameResult;
	DxTexture DepthBuffer;

	DxRenderTarget HdrRenderTarget;

private:
	static DxRenderer* _instance;

	RenderCamera _camera = {};
	DxMesh _mesh = {};
	SubmeshInfo _submesh = {};

	TonemapCb _tonemap = DefaultTonemapParameters();

	D3D12_VIEWPORT _viewport;

	DxTexture _hdrColorTexture;
	DxDescriptorHandle _hdrColorTextureSrv;

	uint32 _renderWidth;
	uint32 _renderHeight;
	uint32 _windowWidth;
	uint32 _windowHeight;

	DxRenderTarget _windowRenderTarget;
	DxDescriptorHandle _frameResultSrv;
	DxTexture _frameResult;

	D3D12_VIEWPORT _windowViewport;

	AspectRatioMode _aspectRatioMode;

	DxPipeline _textureSkyPipeline;
	DxPipeline _proceduralSkyPipeline;
	DxPipeline _presentPipeline;
	DxPipeline _modelPipeline;

	float _clearColor[4];
};