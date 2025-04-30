#pragma once

#include "window/d3dApp.h"
#include "utils/MathHelper.h"
#include "utils/UploadBuffer.h"

struct ObjectConstants;

class BoxApp : public D3DApp {
public:
    BoxApp() = default;
    ~BoxApp() override = default;

    bool Initialize() override;
private:
    void OnResize() override;
    void Update(const GameTimer &gt) override;
    void Draw(const GameTimer &gt) override;

    void MouseDawnHandle(WPARAM btnState, int x, int y) override;
    void MouseUpHandle(WPARAM btnState, int x, int y) override;
    void MouseMoveHandle(WPARAM btnState, int x, int y) override;

    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> _pRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _pCbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> _pObjectCB = nullptr;
    std::unique_ptr<MeshGeometry> _pBoxGeometry = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> _pVSByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> _pPSByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> _pPSO = nullptr;

    DirectX::XMFLOAT4X4 _word = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();

    float _fTheta = 1.5f * DirectX::XM_PI;
    float _fPhi = DirectX::XM_PIDIV4;
    float _fRadius = 5.f;

    POINT _lastMousePos;
};
