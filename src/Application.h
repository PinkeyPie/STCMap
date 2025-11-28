#pragma once

#include "DirectWindow.h"
#include "GameTimer.h"
#include "core/input.h"
#include "core/camera.h"
#include "core/cameraController.h"
#include "physics/mesh.h"
#include "core/math.h"
#include "directx/DxRenderer.h"
#include "light_source.hlsli"
#include "render/LightSource.h"
#include "render/TrasformationGizmo.h"
#include "render/Raytracing.h"

#include "render/PathTracing.h"

#include "render/Scene.h"

class Application {
public:
	void LoadCustomShaders();
	bool Initialize();
	void Update(const UserInput& input, float dt);

	void SetEnvironment(const char* filename);

	void SerializeToFile(const char* filename);
	void DeserializeFromFile(const char* filename);

	void Run();

	static Application* Instance() {
		return _instance;
	}
	uint32 NumOpenWindows = 0;

private:
	bool NewFrame();
	uint64 RenderToWindow(float* clearColor);

	void SetSelectedEntityEulerRotation();
	void SetSelectedEntity(SceneEntity entity);
	void DrawSceneHierarchy();
	void DrawSettings(float dt);

	void ResetRenderPasses();
	void SubmitRenderPasses();
	void HandleUserInput(const UserInput& input, float dt);

	void AssignShadowMapViewports();

	bool HandleWindowsMessages();

	static Application* _instance;
	bool _running = false;

	RaytracingTlas _raytracingTlas;
	PathTracer _pathTracer;

	Ptr<PbrEnvironment> _environment;

	Ptr<DxBuffer> _pointLightBuffer[NUM_BUFFERED_FRAMES];
	Ptr<DxBuffer> _spotLightBuffer[NUM_BUFFERED_FRAMES];
	Ptr<DxBuffer> _decalBuffer[NUM_BUFFERED_FRAMES];

	std::vector<PointLightCb> _pointLights;
	std::vector<SpotLightCb> _spotLights;
	std::vector<PbrDecalCb> _decals;

	Ptr<DxTexture> _decalTexture;

	DirectionalLight _sun;

	DxRenderer* _renderer;

	RenderCamera _camera;
	CameraController _cameraController;

	Scene _appScene;
	SceneEntity _selectedEntity;
	vec3 _selectedEntityEulerRotation;

	OpaqueRenderPass _opaqueRenderPass;
	TransparentRenderPass _transparentRenderPass;
	SunShadowRenderPass _sunShadowRenderPass;
	SpotShadowRenderPass _spotShadowRenderPasses[2];
	PointShadowRenderPass _pointShadowRenderPasses[2];
	OverlayRenderPass _overlayRenderPass;

	UserInput _input = {};
	DirectWindow _mainWindow = {};
	GameTimer _timer;
};
