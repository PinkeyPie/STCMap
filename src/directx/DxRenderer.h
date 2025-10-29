#pragma once

#include "DxPipeline.h"
#include "DxRenderPrimitives.h"
#include "DxRenderTarget.h"
#include "../core/camera.h"

#include "math.h"
#include "present_rs.hlsli"
#include "../physics/mesh.h"

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
	EGizmoTypeTranslation,
	EGizmoTypeRotation,
	EGizmoTypeScale,

	EgizmoTypeCount
};

static const char* gizmoTypeNames[] = {
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
	DxTexture FrameResult;

	DxCbvSrvUavDescriptorHeap GlobalDescriptorHeap;

private:
	static DxRenderer* _instance;

	RenderCamera _camera = {};

	TonemapCb _tonemap = DefaultTonemapParameters();

	D3D12_VIEWPORT _windowViewport;
	D3D12_VIEWPORT _viewport;

	DxRenderTarget _windowRenderTarget;
	DxRenderTarget _hdrRenderTarget;

	DxTexture _hdrColorTexture;
	DxDescriptorHandle _hdrColorTextureSrv;
	DxTexture _depthBuffer;

	uint32 _renderWidth;
	uint32 _renderHeight;
	uint32 _windowWidth;
	uint32 _windowHeight;

	AspectRatioMode _aspectRatioMode;

	DxPipeline _textureSkyPipeline;
	DxPipeline _proceduralSkyPipeline;
	DxPipeline _presentPipeline;
	DxPipeline _modelPipeline;
	DxPipeline _modelDepthOnlyPipeline;
	DxPipeline _outlinePipeline;

	CompositeMesh _sceneMesh;
	DxTexture _meshAlbedoTex;
	DxTexture _meshNormalTex;
	DxTexture _meshRoughTex;
	DxTexture _meshMetalTex;
	DxDescriptorHandle _textureSRV;

	trs _meshTransform;

	DxTexture _whiteTexture;
	DxDescriptorHandle _whiteTextureSRV;

	DxMesh _skyMesh;
	DxMesh _gizmoMesh;
	PbrEnvironment _environment;

	static union {
		struct {
			SubmeshInfo TranslationGizmoSubmesh;
			SubmeshInfo RotationGizmoSubmesh;
			SubmeshInfo ScaleGizmoSubmesh;
		};

		SubmeshInfo gizmoSubmeshes[3];
	};

	quat _gizmoRotations[] = {
		quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)),
		quat::identity,
		quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f))
	};

	vec4 _gizmoColors[] = {
		vec4(1.f, 0.f, 0.f, 1.f),
		vec4(0.f, 1.f, 0.f, 1.f),
		vec4(0.f, 0.f, 1.f, 1.f)
	};

	DxTexture _brdfTex;
	DxDescriptorHandle _brdfTexSRV;

	float _clearColor[4];
};