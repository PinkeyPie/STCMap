#pragma once

#include "window/d3dApp.h"
#include "utils/MathHelper.h"
#include "utils/UploadBuffer.h"
#include "utils/GeometryGenerator.h"
#include "../utility/FrameResource.h"

// LightWeight structure stores parameters to draw a shape
struct RenderItem;

class ShapesWindow : public D3DApp {
public:
	ShapesWindow() = default;
	ShapesWindow(const ShapesWindow& other) = delete;
	ShapesWindow& operator=(const ShapesWindow& other) = delete;
	~ShapesWindow();

	bool Initialize() override;

private:
	void ResizeHandle() override;
	void Update(const GameTimer& gt) override;
	void Draw(const GameTimer& gt) override;

	void MouseDawnHandle(WPARAM btnState, int x, int y) override;
	void MouseUpHandle(WPARAM btnState, int x, int y) override;
	void MouseMoveHandle(WPARAM btnState, int x, int y) override;

	void KeyboardInputHandle(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildDescriptorHeaps();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& rItems);

private:
	std::vector<std::unique_ptr<FrameResource>> _frameResources;
	FrameResource* _pCurrFrameResource = nullptr;
	int _nCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> _pRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _pCbvHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _pSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _psos;

	// List of all render items
	std::vector<std::unique_ptr<RenderItem>> _allRItems;

	// Render items divided by PSO
	std::vector<RenderItem*> _opaqueRItems;

	PassConstants _mainPassCB;

	UINT _nPassCbvOffset = 0;

	bool _bIsWireframe = false;

	DirectX::XMFLOAT3 _eyePos = { 0.f, 0.f, 0.f };
	DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();

	float _fTheta = 1.f * DirectX::XM_PI;
	float _fPhi = 0.2f * DirectX::XM_PI;
	float _fRadius = 15.f;

	POINT _lastMousePos;
};
