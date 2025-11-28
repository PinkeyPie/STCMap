#include "Application.h"
#include "physics/geometry.h"
#include "directx/DxTexture.h"
#include "core/random.h"
#include "core/color.h"
#include "directx/DxContext.h"
#include "animation/skinning.h"
#include "core/threading.h"
#include "render/MeshShader.h"
#include "directx/DxBarrierBatcher.h"
#include "directx/DxCommandList.h"
#include "render/ShadowMapCache.h"
#include <iostream>
#include "render/pbr.hpp"


Application* Application::_instance = new Application{};

namespace {
	LONG NTAPI HandleVectoredException(PEXCEPTION_POINTERS exceptionInfo) {
		PEXCEPTION_RECORD exceptionRecord = exceptionInfo->ExceptionRecord;

		switch (exceptionRecord->ExceptionCode) {
		case DBG_PRINTEXCEPTION_WIDE_C:
		case DBG_PRINTEXCEPTION_C:
			if (exceptionRecord->NumberParameters >= 2) {
				ULONG len = (ULONG)exceptionRecord->ExceptionInformation[0];

				union {
					ULONG_PTR up;
					PCWSTR pwz;
					PCSTR psz;
				};

				up = exceptionRecord->ExceptionInformation[1];

				HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);

				if (exceptionRecord->ExceptionCode == DBG_PRINTEXCEPTION_C) {
					// Localized text will be incorrect displayed, if used not CP_OEMCP encoding.
					// WriteConsoleA(hOut, psz, len, &len, 0);

					// assume CP_ACP encoding
					if (ULONG n = MultiByteToWideChar(CP_ACP, 0, psz, len, nullptr, 0)) {
						PWSTR wz = (PWSTR)alloca(n * sizeof(WCHAR));

						if (len = MultiByteToWideChar(CP_ACP, 0, psz, len, wz, n)) {
							pwz = wz;
						}
					}
				}

				if (len) {
					WriteConsoleW(hOut, pwz, len - 1, &len, nullptr);
				}
			}

			return EXCEPTION_CONTINUE_EXECUTION;
		}

		return EXCEPTION_CONTINUE_SEARCH;
	}

	RaytracingObjectType DefineBlasFromMesh(const Ptr<CompositeMesh>& mesh, PathTracer& pathTracer) {
		DxContext& dxContext = DxContext::Instance();
		if (dxContext.RaytracingSupported()) {
			RaytracingBlasBuilder blasBuilder;
			std::vector<Ptr<PbrMaterial>> raytracingMaterials;

			for (auto& sm : mesh->Submeshes) {
				blasBuilder.Push(mesh->Mesh.VertexBuffer, mesh->Mesh.IndexBuffer, sm.Info);
				raytracingMaterials.push_back(sm.Material);
			}

			Ptr<RaytracingBlas> blas = blasBuilder.Finish();
			RaytracingObjectType type = pathTracer.DefineObjectType(blas, raytracingMaterials);
			return type;
		}
		else {
			return {};
		}
	}
}

struct RasterComponent {
	Ptr<CompositeMesh> Mesh;
};

struct AnimationComponent {
	float Time;

	uint32 AnimationIndex = 0;

	Ptr<DxVertexBuffer> VB;
	SubmeshInfo SMs[16];

	Ptr<DxVertexBuffer> PrevFrameVB;
	SubmeshInfo PrefFrameSMs[16];
};

struct RaytraceComponent {
	RaytracingObjectType Type;
};

void Application::LoadCustomShaders() {
	ShadowMapLightInfo info;
	info.Viewport.CpuVP[0] = 2048;
	info.Viewport.CpuVP[1] = 2048;
	info.Viewport.CpuVP[2] = 4096;
	info.Viewport.CpuVP[3] = 4096;

	info.LightMovedOrAppeared = false;
	info.GeometryInRangeMoved = false;

	ShadowMapLightInfo::TestShadowMapCache(&info, 1);

	if (DxContext::Instance().MeshShaderSupported()) {
		InitializeMeshShader();
	}
}

bool Application::NewFrame() {
	bool result = HandleWindowsMessages();
	// Quit when escape is pressed, but not if in combination with ctrl or shift. This combination is usually pressed to open the task manager.

	return result;
}

bool Application::HandleWindowsMessages() {
	MSG msg = {0};

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			_running = false;
			break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (NumOpenWindows == 0) {
		_running = false;
	}

	return _running;
}

uint64 Application::RenderToWindow(float* clearColor) {
	DxResource backBuffer = _mainWindow.GetCurrentBackBuffer();
	DxRtvDescriptorHandle rtv = _mainWindow.Rtv();
	DxResource frameResult = DxRenderer::Instance()->FrameResult->Resource();
	
	DxCommandList* cl = DxContext::Instance().GetFreeRenderCommandList();

	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)_mainWindow.ClientWidth, (float)_mainWindow.ClientHeight);
	cl->SetViewport(viewport);

	cl->TransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cl->ClearRTV(rtv, 0.f, 0.f, 0.f);
	cl->SetRenderTarget(&rtv, 1, 0);
	DxBarrierBatcher(cl).Transition(backBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RESOLVE_DEST)
				      .Transition(frameResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	cl->ResolveSubresource(backBuffer, 0, frameResult, 0, _mainWindow.GetBackBufferFormat());
	DxBarrierBatcher(cl).Transition(frameResult, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
				      .Transition(backBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT);

	uint64 result = DxContext::Instance().ExecuteCommandList(cl);

	_mainWindow.SwapBuffers();

	return result;
}

bool Application::Initialize() {
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	AddVectoredExceptionHandler(TRUE, HandleVectoredException);

	DxContext& dxContext = DxContext::Instance();
	JobFactory::Instance()->InitializeJobSystem();

	constexpr ColorDepth colorDepth = EColorDepth8;
	DXGI_FORMAT screenFormat = (colorDepth == EColorDepth8) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R10G10B10A2_UNORM;

	constexpr uint32 initialWidth = 1280;
	constexpr uint32 initialHeight = 800;

	if (not _mainWindow.Initialize(TEXT("Main Window"), initialWidth, initialHeight, colorDepth)) {
		return false;
	}
	NumOpenWindows++;
	LoadCustomShaders();
	InitializeTransformationGizmos();

	_renderer = DxRenderer::Instance();
	_renderer->Initialize(screenFormat, initialWidth, initialHeight, true);

	_camera.InitializeIngame(vec3(0.f, 30.f, 40.f), quat::identity, deg2rad(70.f), 0.1f);
	_cameraController.Initialize(&_camera);

	if (dxContext.RaytracingSupported()) {
		_pathTracer.Initialize();
		_raytracingTlas.Initialize();
	}

	// Sponza
	auto sponzaMesh = LoadMeshFromFile("assets/meshes/sponza.obj");
	if (sponzaMesh) {
		auto sponzaBlas = DefineBlasFromMesh(sponzaMesh, _pathTracer);
		_appScene.CreateEntity("Sponza").AddComponent<trs>(vec3(0.f, 0.f, 0.f), quat::identity, 0.01f)
		.AddComponent<RasterComponent>(sponzaMesh).AddComponent<RaytraceComponent>(sponzaBlas);
	}

	// Max caufield
	auto maxMesh = LoadAnimatedMeshFromFile("assets/meshes/max.fbx");
	if (maxMesh) {
		maxMesh->Submeshes[0].Material = CreatePBRMaterial("", nullptr, nullptr, nullptr, vec4(0.f), vec4(1.f), 0.f, 0.f);
	}

	// Unreal Mannequin.
	auto unrealMesh = LoadAnimatedMeshFromFile("assets/meshes/unreal_mannequin.fbx");
	if (unrealMesh) {
		unrealMesh->Skeleton.PushAssimpAnimationsInDirectory("assets/animations");
	}

	if (maxMesh) {
		_appScene.CreateEntity("Max 1").AddComponent<trs>(vec3(-5.f, 0.f, -1.f), quat::identity)
		.AddComponent<RasterComponent>(maxMesh).AddComponent<AnimationComponent>(1.5f);

		_appScene.CreateEntity("Max 2").AddComponent<trs>(vec3(0.f, 0.f, -2.f), quat::identity)
		.AddComponent<RasterComponent>(maxMesh).AddComponent<AnimationComponent>(0.f);

		_appScene.CreateEntity("Max 3").AddComponent<trs>(vec3(0.f, 0.f, -2.f), quat::identity)
		.AddComponent<RasterComponent>(maxMesh).AddComponent<AnimationComponent>();
	}

	if (unrealMesh) {
		_appScene.CreateEntity("Mannequin").AddComponent<trs>(vec3(-2.5f, 0.f, -1.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)), 0.019f)
		.AddComponent<RasterComponent>(unrealMesh).AddComponent<AnimationComponent>(0.f);
	}

	if (dxContext.RaytracingSupported()) {
		_pathTracer.Finish();
	}

	SetEnvironment("assets/textures/aircraft_workshop_01_4k.hdr");

	RandomNumberGenerator rng = { 14878213 };

	_spotLights.resize(2);

	_spotLights[0].Initialize(
		{ 2.f, 3.f, 0.f },
		{ 1.f, 0.f, 0.f },
		RandomRGB(rng) * 5.f,
		deg2rad(20.f),
		deg2rad(30.f),
		25.f,
		0
	);

	_spotLights[1].Initialize(
		{ -2.f, 3.f, 0.f },
		{ -1.f, 0.f, 0.f },
		RandomRGB(rng) * 5.f,
		deg2rad(20.f),
		deg2rad(30.f),
		25.f,
		1
	);

	_pointLights.resize(1);

	_pointLights[0].Initialize(
		{ 0.f, 8.f, 0.f },
		RandomRGB(rng),
		10,
		0
	);

	TextureFactory* textureFactory = TextureFactory::Instance();
	_decalTexture = textureFactory->LoadTextureFromFile("assets/textures/explosion.png");

	if (_decalTexture) {
		_decals.resize(5);

		for (uint32 i = 0; i < _decals.size(); i++) {
			_decals[i].Initialize(
				vec3(-7.f + i * 3, 2.f, -3.f),
				vec3(3.f, 0.f, 0.f),
				vec3(0.f, 3.f, 0.f),
				vec3(0.f, 0.f, -0.75f),
				vec4(1.f, 1.f, 1.f, 1.f),
				1.f,
				1.f,
				vec4(0.f, 0.f, 1.f, 1.f)
			);
		}

		SET_NAME(_decalTexture->Resource(), "Decal");
	}

	_sun.Direction = normalize(vec3(-0.6f, -1.f, -0.3f));
	_sun.Color = vec3(1.f, 0.93f, 0.76f);
	_sun.Intensity = 50.f;

	_sun.NumShadowCascades = 3;
	_sun.ShadowDimensions = 2048;
	_sun.CascadeDistances = vec4(9.f, 39.f, 74.f, 10000.f);
	_sun.Bias = vec4(0.000049f, 0.000114f, 0.000082f, 0.0035f);
	_sun.BlendDistances = vec4(3.f, 3.f, 10.f, 10.f);

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; i++) {
		_pointLightBuffer[i] = DxBuffer::CreateUpload(sizeof(PointLightCb), 512, 0);
		_spotLightBuffer[i] = DxBuffer::CreateUpload(sizeof(SpotLightCb), 512, 0);
		_decalBuffer[i] = DxBuffer::CreateUpload(sizeof(PbrDecalCb), 512, 0);

		SET_NAME(_pointLightBuffer[i]->Resource, "Point lights");
		SET_NAME(_spotLightBuffer[i]->Resource, "Spot lights");
		SET_NAME(_decalBuffer[i]->Resource, "Decals");
	}

	_timer.Reset();

	return true;
}

void Application::SetSelectedEntityEulerRotation() {
	if (_selectedEntity and _selectedEntity.HasComponent<trs>()) {
		_selectedEntityEulerRotation = quatToEuler(_selectedEntity.GetComponent<trs>().rotation);
		_selectedEntityEulerRotation.x = rad2deg(AngleToZeroToTwoPi(_selectedEntityEulerRotation.x));
		_selectedEntityEulerRotation.y = rad2deg(AngleToZeroToTwoPi(_selectedEntityEulerRotation.y));
		_selectedEntityEulerRotation.z = rad2deg(AngleToZeroToTwoPi(_selectedEntityEulerRotation.z));
	}
}

void Application::SetSelectedEntity(SceneEntity entity) {
	_selectedEntity = entity;
	SetSelectedEntityEulerRotation();
}

void Application::ResetRenderPasses() {
	_opaqueRenderPass.Reset();
	_overlayRenderPass.Reset();
	_transparentRenderPass.Reset();
	_sunShadowRenderPass.Reset();

	for (uint32 i = 0; i < std::size(_spotShadowRenderPasses); i++) {
		_spotShadowRenderPasses[i].Reset();
	}
	for (uint32 i = 0; i < std::size(_pointShadowRenderPasses); i++) {
		_pointShadowRenderPasses[i].Reset();
	}
}

void Application::SubmitRenderPasses() {
	_renderer->SubmitRenderPass(&_opaqueRenderPass);
	_renderer->SubmitRenderPass(&_transparentRenderPass);
	_renderer->SubmitRenderPass(&_overlayRenderPass);
	_renderer->SubmitRenderPass(&_sunShadowRenderPass);

	for (uint32 i = 0; i < std::size(_spotShadowRenderPasses); i++) {
		_renderer->SubmitRenderPass(&_spotShadowRenderPasses[i]);
	}

	for (uint32 i = 0; i < std::size(_pointShadowRenderPasses); i++) {
		_renderer->SubmitRenderPass(&_pointShadowRenderPasses[i]);
	}
}

void Application::HandleUserInput(const UserInput &input, float dt) {
	if (_input.Keyboard['F'].PressEvent and _selectedEntity) {
		auto& raster = _selectedEntity.GetComponent<RasterComponent>();
		auto& transform = _selectedEntity.GetComponent<trs>();

		auto aabb = raster.Mesh->AABB;
		aabb.MinCorner *= transform.scale;
		aabb.MaxCorner *= transform.scale;

		aabb.MinCorner += transform.position;
		aabb.MaxCorner += transform.position;

		_cameraController.CenterCameraOnObject(aabb);
	}

	bool inputCaptured = _cameraController.Update(input, _renderer->RenderWidth, _renderer->RenderHeight, dt);
	if (inputCaptured) {
		_pathTracer.NumAveragedFrames = 0;
	}

	if (_selectedEntity and _selectedEntity.HasComponent<trs>()) {
		trs& transform = _selectedEntity.GetComponent<trs>();

		static ETransformationType type = ETransformationTypeTranslation;
		static ETransformationSpace space = ETransformationGlobal;
		if (ManipulateTransformation(transform, type, space, _camera, input, !inputCaptured, &_overlayRenderPass)) {
			SetSelectedEntityEulerRotation();
			inputCaptured = true;
		}
	}

	if (!inputCaptured and input.Mouse.Left.ClickEvent) {
		if (_renderer->HoveredObjectID != 0xffff) {
			SetSelectedEntity({ _renderer->HoveredObjectID, _appScene });
		}
		else {
			SetSelectedEntity({});
		}
		inputCaptured = true;
	}
}

void Application::DrawSceneHierarchy() {}

void Application::DrawSettings(float dt) {}

void Application::AssignShadowMapViewports() {}

void Application::Update(const UserInput &input, float dt) {
	ResetRenderPasses();
	HandleUserInput(input, dt);

	DrawSceneHierarchy();
	DrawSettings(dt);

	_sun.UpdateMatrices(_camera);

	_renderer->SetCamera(_camera);
	_renderer->SetSun(_sun);
	_renderer->SetEnvironment(_environment);

	DxContext& dxContext = DxContext::Instance();
	if (_renderer->Mode == ERendererModeRasterized) {
		if (dxContext.MeshShaderSupported()) {
			// TestRenderMeshShader(&_overlayRenderPass);
		}

		_spotShadowRenderPasses[0].ViewProjMatrix = GetSpotlightViewProjMatrix(_spotLights[0]);
		_spotShadowRenderPasses[1].ViewProjMatrix = GetSpotlightViewProjMatrix(_spotLights[1]);
		_pointShadowRenderPasses[0].LightPosition = _pointLights[0].Position;
		_pointShadowRenderPasses[0].MaxDistance = _pointLights[0].Radius;

		AssignShadowMapViewports();

		// Upload and set lights
		if (_pointLights.size()) {
			_pointLightBuffer[dxContext.BufferedFrameId()]->UpdateUploadData(_pointLights.data(), (uint32)(sizeof(PointLightCb) * _pointLights.size()));
			_renderer->SetPointLights(_pointLightBuffer[dxContext.BufferedFrameId()], (uint32)_pointLights.size());
		}
		if (_spotLights.size()) {
			_spotLightBuffer[dxContext.BufferedFrameId()]->UpdateUploadData(_spotLights.data(), (uint32)(sizeof(SpotLightCb) * _spotLights.size()));
			_renderer->SetSpotLights(_spotLightBuffer[dxContext.BufferedFrameId()], (uint32)_spotLights.size());
		}
		if (_decals.size()) {
			_decalBuffer[dxContext.BufferedFrameId()]->UpdateUploadData(_decals.data(), (uint32)(sizeof(PbrDecalCb) * _decals.size()));
			_renderer->SetDecals(_decalBuffer[dxContext.BufferedFrameId()], (uint32)_decals.size(), _decalTexture);
		}

		ThreadJobContext context;

		// Skin animated meshes
		_appScene.group<AnimationComponent>(entt::get<RasterComponent>)
		.each([dt, &context](AnimationComponent& anim, RasterComponent& raster) {
			anim.Time += dt;
			const DxMesh& mesh = raster.Mesh->Mesh;
			AnimationSkeleton& skeleton = raster.Mesh->Skeleton;

			auto [vb, vertexOffset, skinningMatrices] = SkinObject(mesh.VertexBuffer, (uint32)skeleton.Joints.size());

			mat4* mats = skinningMatrices;
			context.AddWork([&skeleton, &anim, mats]() {
				trs localTransforms[128];
				skeleton.SampleAnimation(skeleton.Clips[anim.AnimationIndex].Name, anim.Time, localTransforms);
				skeleton.GetSkinningMatricesFromLocalTransforms(localTransforms, mats);
			});

			anim.PrevFrameVB = anim.VB;
			anim.VB = vb;

			uint32 numSubmeshes = (uint32)raster.Mesh->Submeshes.size();
			for (uint32 i = 0; i < numSubmeshes; i++) {
				anim.PrefFrameSMs[i] = anim.SMs[i];

				anim.SMs[i] = raster.Mesh->Submeshes[i].Info;
				anim.SMs[i].BaseVertex += vertexOffset;
			}
		});

		// Submit render calls.
		_appScene.group<RasterComponent>(entt::get<trs>).each([this](entt::entity entityHandle, RasterComponent& raster, trs& transform) {
			const DxMesh& mesh = raster.Mesh->Mesh;
			mat4 m = trsToMat4(transform);

			SceneEntity entity = { entityHandle, _appScene };
			bool outline = _selectedEntity == entity;

			if (entity.HasComponent<AnimationComponent>()) {
				auto& anim = entity.GetComponent<AnimationComponent>();

				uint32 numSubmeshes = (uint32)raster.Mesh->Submeshes.size();

				for (uint32 i = 0; i < numSubmeshes; ++i) {
					SubmeshInfo submesh = anim.SMs[i];
					SubmeshInfo prevFrameSubmesh = anim.PrefFrameSMs[i];

					const Ptr<PbrMaterial>& material = raster.Mesh->Submeshes[i].Material;

					if (material->AlbedoTint.a < 1.f) {
						_transparentRenderPass.RenderObject(anim.VB, mesh.IndexBuffer, submesh, material, m, outline);
					}
					else {
						_opaqueRenderPass.RenderAnimatedObject(anim.VB, anim.PrevFrameVB, mesh.IndexBuffer, submesh, prevFrameSubmesh, material, m, m,
							(uint32)entityHandle, outline);
						_sunShadowRenderPass.RenderObject(0, anim.VB, mesh.IndexBuffer, submesh, m);
						_spotShadowRenderPasses[0].RenderObject(anim.VB, mesh.IndexBuffer, submesh, m);
						_spotShadowRenderPasses[1].RenderObject(anim.VB, mesh.IndexBuffer, submesh, m);
						_pointShadowRenderPasses[0].RenderObject(anim.VB, mesh.IndexBuffer, submesh, m);
					}
				}
			}
			else {
				for (auto& sm : raster.Mesh->Submeshes) {
					SubmeshInfo submesh = sm.Info;
					const Ptr<PbrMaterial>& material = sm.Material;

					if (material->AlbedoTint.a < 1.f) {
						_transparentRenderPass.RenderObject(mesh.VertexBuffer, mesh.IndexBuffer, submesh, material, m, outline);
					}
					else {
						_opaqueRenderPass.RenderStaticObject(mesh.VertexBuffer, mesh.IndexBuffer, submesh, material, m, (uint32)entityHandle, outline);
						_sunShadowRenderPass.RenderObject(0, mesh.VertexBuffer, mesh.IndexBuffer, submesh, m);
						_spotShadowRenderPasses[0].RenderObject(mesh.VertexBuffer, mesh.IndexBuffer, submesh, m);
						_spotShadowRenderPasses[1].RenderObject(mesh.VertexBuffer, mesh.IndexBuffer, submesh, m);
						_pointShadowRenderPasses[0].RenderObject(mesh.VertexBuffer, mesh.IndexBuffer, submesh, m);
					}
				}
			}
		});

		context.WaitForWorkCompletion();
		SubmitRenderPasses();
	}
	else {
		if (dxContext.RaytracingSupported()) {
			_raytracingTlas.Reset();

			_appScene.group<RaytraceComponent>(entt::get<trs>)
			.each([this](entt::entity entityHandle, RaytraceComponent& raytrace, trs& transform) {
				_raytracingTlas.Instantiate(raytrace.Type, transform);
			});

			_raytracingTlas.Build();

			_renderer->SetRaytracer(&_pathTracer, &_raytracingTlas);
		}
	}
}

void Application::SetEnvironment(const char *filename) {
	_environment = CreateEnvironment(filename);
	_pathTracer.NumAveragedFrames = 0;

	if (not _environment) {
		std::cout << "Could not load environment '" << filename << "'. Renderer will use procedural sky box. Procedural sky boxes currently cannot contribute to global illumnation, so expect very dark lighting." << std::endl;
	}
}

void Application::Run() {
	DxContext& dxContext = DxContext::Instance();

	uint64 fenceValues[NUM_BUFFERED_FRAMES] = {};
	uint64 frameId = 0;

	fenceValues[NUM_BUFFERED_FRAMES - 1] = dxContext.RenderQueue.Signal();

	_running = true;
	while (NewFrame()) {
		dxContext.RenderQueue.WaitForFence(fenceValues[_mainWindow.CurrentBackBufferIndex()]);
		dxContext.NewFrame(frameId);

		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtv = _mainWindow.Rtv();
		const DxResource backBuffer = _mainWindow.GetCurrentBackBuffer();

		_renderer->BeginFrameCommon();
		_renderer->BeginFrame(rtv, backBuffer);
		Update(_input, _timer.DeltaTime());

		_renderer->EndFrameCommon();
		_renderer->EndFrame(_input);

		float clearColor1[] = { 1.f, 0.f, 0.f, 1.f };

		fenceValues[_mainWindow.CurrentBackBufferIndex()] = RenderToWindow(clearColor1);

		_mainWindow.SwapBuffers();
		++frameId;
	}

	dxContext.Quit();
}
