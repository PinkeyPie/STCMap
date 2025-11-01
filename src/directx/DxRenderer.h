#pragma once

#include "DxPipeline.h"
#include "DxRenderPrimitives.h"
#include "DxRenderTarget.h"
#include "../core/camera.h"

#include "math.h"
#include "present_rs.hlsli"
#include "../input.h"
#include "../physics/mesh.h"

#define MAX_NUM_POINT_LIGHTS_PER_FRAME 4096
#define MAX_NUM_SPOT_LIGHTS_PER_FRAME 4096

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

enum GizmoType {
	EGizmoTypeNone,
	EGizmoTypeTranslation,
	EGizmoTypeRotation,
	EGizmoTypeScale,

	EGizmoTypeCount
};

static const char* gizmoTypeNames[] = {
	"None",
	"Translation",
	"Rotation",
	"Scale"
};

struct PbrEnvironment {
	DxTexture Sky;
	DxTexture Prefiltered;
	DxTexture Irradiance;

	DxDescriptorHandle SkySRV;
	DxDescriptorHandle PrefilteredSRV;
	DxDescriptorHandle IrradianceSRV;
};

struct LightCullingBuffers {
	DxBuffer TiledFrustum;
	DxBuffer PointLightBoundingVolumes;
	DxBuffer SpotLightBoundingVolumes;

	DxBuffer OpaqueLightIndexCounter;
	DxBuffer OpaqueLightIndexList;

	DxTexture OpaqueLightGrid;

	uint32 NumTilesX;
	uint32 NumTilesY;

	DxDescriptorHandle ResourceHandle;
	DxDescriptorHandle OpaqueLightGridSRV;

	DxDescriptorHandle OpaqueLightIndexCounterUAV_CPU;
	DxDescriptorHandle OpaqueLightIndexCounterUAV_GPU;
};

class DxRenderer {
public:
	DxRenderer() = default;

	static DxRenderer* Instance() {
		return _instance;
	}
	void Initialize(uint32 width, uint32 height);

	void BeginFrame(uint32 width, uint32 height, float dt);
	void BeginFrame(CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, DxResource renderTarget);
	void HandleInput(const UserInput& input, float dt);
	void RecalculateViewport(bool resizeTextures);
	void FillCameraConstantBuffer(struct CameraCb& cb);
	void AllocateLightCullingBuffers();
	int DummyRender(float dt);
	DxTexture FrameResult;

	DxCbvSrvUavDescriptorHeap GlobalDescriptorHeap;
	DxCbvSrvUavDescriptorHeap GlobalDescriptorHeapCPU;

private:
	static DxRenderer* _instance;

	RenderCamera _camera = {};
	DxDynamicConstantBuffer _cameraCBV;

	TonemapCb _tonemap = DefaultTonemapParameters();

	D3D12_VIEWPORT _windowViewport;
	D3D12_VIEWPORT _viewport;

	DxRenderTarget _windowRenderTarget;
	// DxRenderTarget _hdrRenderTarget;

	// DxTexture _hdrColorTexture;
	DxDescriptorHandle _hdrColorTextureSrv;
	DxTexture _depthBuffer;
	DxDescriptorHandle _depthBufferSRV;

	LightCullingBuffers _lightCullingBuffers;

	uint32 _renderWidth;
	uint32 _renderHeight;
	uint32 _windowWidth;
	uint32 _windowHeight;

	AspectRatioMode _aspectRatioMode = EAspectRatioFree;

	DxPipeline _textureSkyPipeline;
	DxPipeline _proceduralSkyPipeline;
	DxPipeline _presentPipeline;
	DxPipeline _modelPipeline;
	DxPipeline _modelDepthOnlyPipeline;
	DxPipeline _outlinePipeline;
	DxPipeline _flatUnlitPipeline;

	DxPipeline _worldSpaceFrustumPipeline;
	DxPipeline _lightCullingPipeline;

	CompositeMesh _sceneMesh;
	DxTexture _meshAlbedoTex;
	DxTexture _meshNormalTex;
	DxTexture _meshRoughTex;
	DxTexture _meshMetalTex;
	DxDescriptorHandle _textureSRV;

	trs _meshTransform;

	DxTexture _whiteTexture;
	DxDescriptorHandle _whiteTextureSRV;

	DxMesh _gizmoMesh;
	DxMesh _positionOnlyMesh;
	SubmeshInfo _cubeMesh;
	SubmeshInfo _sphereMesh;

	PbrEnvironment _environment;

	static union {
		struct {
			SubmeshInfo NoneGizmoSubmesh;
			SubmeshInfo TranslationGizmoSubmesh;
			SubmeshInfo RotationGizmoSubmesh;
			SubmeshInfo ScaleGizmoSubmesh;
		};

		SubmeshInfo gizmoSubmeshes[4];
	};

	quat _gizmoRotations[3] = {
		quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)),
		quat::identity,
		quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f))
	};

	vec4 _gizmoColors[3] = {
		vec4(1.f, 0.f, 0.f, 1.f),
		vec4(0.f, 1.f, 0.f, 1.f),
		vec4(0.f, 0.f, 1.f, 1.f)
	};

	DxTexture _brdfTex;
	DxDescriptorHandle _brdfTexSRV;

	float _clearColor[4];
};