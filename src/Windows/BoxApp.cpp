#include "BoxApp.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

struct ObjectConstants {
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

bool BoxApp::Initialize() {
    if(not D3DApp::Initialize()) {
        return false;
    }

    // Reset the command list to prep for inititalization commands;
    ThrowIfFailed(_pCommandList->Reset(_pDirectCommandListAlloc.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

    // Execute the initialization commands
    ThrowIfFailed(_pCommandList->Close());
    ID3D12CommandList* cmdLists[] = { _pCommandList.Get() };
    _pCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // Wait until initialization finished
    FlushCommandQueue();

    return true;
}

void BoxApp::OnResize() {
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompile the projection matrix
    XMMATRIX proj = XMMatrixPerspectiveLH(0.25f * MathHelper::Pi, AspectRatio(), 1.f, 1000.f);
    XMStoreFloat4x4(&_proj, proj);
}

void BoxApp::Update(const GameTimer &gt) {
    // Convert Spherical to Cartesian coordinates
    float x = _fRadius * sinf(_fPhi) * cosf(_fTheta);
    float z = _fRadius * sinf(_fPhi) * cosf(_fTheta);
    float y = _fRadius * cosf(_fPhi);

    // Build the view matrix
    XMVECTOR pos = XMVectorSet(x, y, z, 1.f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&_view, view);

    XMMATRIX world = XMLoadFloat4x4(&_word);
    XMMATRIX proj = XMLoadFloat4x4(&_proj);
    XMMATRIX worldViewProj = world * view * proj;

    // Update the constant buffer with the latest worldViewProj matrix
    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    _pObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer &gt) {
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU
    ThrowIfFailed(_pDirectCommandListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList
    // Reusing the command list reuses memory
    ThrowIfFailed(_pCommandList->Reset(_pDirectCommandListAlloc.Get(), _pPSO.Get()));

    _pCommandList->RSSetViewports(1, &_screenViewport);
    _pCommandList->RSSetScissorRects(1, &_scissorRect);

    // Indicate a state transition on the resource usage
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    _pCommandList->ResourceBarrier(1, &barrier);

    // Clear the back buffer and depth buffer
    _pCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    _pCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

    // Specify the buffers we are going to render to
    const D3D12_CPU_DESCRIPTOR_HANDLE backBufferView = CurrentBackBufferView();
    const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = DepthStencilView();
    _pCommandList->OMSetRenderTargets(1, &backBufferView, true, &depthStencilView);

    ID3D12DescriptorHeap* descriptorHeaps[] = { _pCbvHeap.Get() };
    _pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    _pCommandList->SetGraphicsRootSignature(_pRootSignature.Get());

    const D3D12_VERTEX_BUFFER_VIEW vertexes = _pBoxGeometry->VertexBufferView();
    const D3D12_INDEX_BUFFER_VIEW indexes = _pBoxGeometry->IndexBufferView();
    _pCommandList->IASetVertexBuffers(0, 1, &vertexes);
    _pCommandList->IASetIndexBuffer(&indexes);
    _pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    _pCommandList->SetGraphicsRootDescriptorTable(9, _pCbvHeap->GetGPUDescriptorHandleForHeapStart());

    _pCommandList->DrawIndexedInstanced(_pBoxGeometry->DrawArgs["box"].IndexCount, 1, 0, 0, 0);

    // Indicate a state transition on the resource usage
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    _pCommandList->ResourceBarrier(1, &barrier);

    // Done recording commands
    ThrowIfFailed(_pCommandList->Close());

    // Add the command list to the queue for execution
    ID3D12CommandList* cmdLists[] = { _pCommandList.Get() };
    _pCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // swap back buffer and front buffer
    ThrowIfFailed(_pSwapChain->Present(0, 0));
    _nCurrBackBuffer = (_nCurrBackBuffer + 1) % SwapChainBufferCount;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}

void BoxApp::MouseDawnHandle(WPARAM btnState, int x, int y) {
    _lastMousePos.x = x;
    _lastMousePos.y = y;

    SetCapture(hwnd);
}

void BoxApp::MouseUpHandle(WPARAM btnState, int x, int y) {
    ReleaseCapture();
}

void BoxApp::MouseMoveHandle(WPARAM btnState, int x, int y) {
    if((btnState & MK_LBUTTON) != 0) {
        // Make each pixel correspond to a quarter of degree
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - _lastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - _lastMousePos.y));

        // Update angles based on input to orbit camera around box
        _fTheta += dx;
        _fPhi += dy;

        // Restrict the angle phi
        _fPhi = MathHelper::Clamp(_fPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0) {
        // Make each pixel correspond to 0.005 until in the scene
        float dx = 0.005f * static_cast<float>(x - _lastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - _lastMousePos.y);

        // Update the camera radius based on input
        _fRadius += dx - dy;

        // Restrict radius
        _fRadius = MathHelper::Clamp(_fRadius, 3.f, 15.f);
    }

    _lastMousePos.x = x;
    _lastMousePos.y = y;
}

void BoxApp::BuildDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_pDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(_pCbvHeap.GetAddressOf())));
}

void BoxApp::BuildConstantBuffers() {
    _pObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(_pDevice.Get(), 1, true);

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = _pObjectCB->Resource()->GetGPUVirtualAddress();
    // Offset to the ith object constant in the buffer
    int boxCBufIndex = 0;
    cbAddress += boxCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    _pDevice->CreateConstantBufferView(&cbvDesc, _pCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildRootSignature() {
    // Shader programs typically require resources as input (constant buffers,
    // textures, samplers).  The root signature defines the resources the shader
    // programs expect.  If we think of the shader programs as a function, and
    // the input resources as function parameters, then the root signature can be
    // thought of as defining the function signature.

    // Root parameter can be a table, root descriptor or root constants
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    // Create a single descriptor table of CBVs
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    // A root signature is an array of root parameters
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if(errorBlob != nullptr) {
        OutputDebugString((TCHAR*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(_pDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_pRootSignature.GetAddressOf())));
}

void BoxApp::BuildShadersAndInputLayout() {
    HRESULT hr = S_OK;

    TCHAR AbsolutePath[MAX_PATH];
    TCHAR ShaderPath[] = TEXT("\\shaders\\color.hlsl");
    DWORD dwRet = GetCurrentDirectory(MAX_PATH, AbsolutePath);
    tcscat(AbsolutePath, ShaderPath);

    _pVSByteCode = d3dUtil::CompileShader(AbsolutePath, nullptr, "VS", "vs_5_1");
    _pPSByteCode = d3dUtil::CompileShader(AbsolutePath, nullptr, "PS", "ps_5_1");

    _inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void BoxApp::BuildBoxGeometry() {
    std::array<Vertex, 8> vertexes = {
        Vertex({XMFLOAT3(-1.f, -1.f, -1.f), XMFLOAT4(Colors::White)}),
        Vertex({XMFLOAT3(-1.f, +1.f, -1.f), XMFLOAT4(Colors::Black)}),
        Vertex({XMFLOAT3(+1.f, +1.f, -1.f), XMFLOAT4(Colors::Red)}),
        Vertex({XMFLOAT3(+1.f, -1.f, -1.f), XMFLOAT4(Colors::Green)}),
        Vertex({XMFLOAT3(-1.f, -1.f, +1.f), XMFLOAT4(Colors::Blue)}),
        Vertex({XMFLOAT3(-1.f, +1.f, +1.f), XMFLOAT4(Colors::Yellow)}),
        Vertex({XMFLOAT3(+1.f, +1.f, +1.f), XMFLOAT4(Colors::Cyan)}),
        Vertex({XMFLOAT3(+1.f, -1.f, +1.f), XMFLOAT4(Colors::Magenta)}),
    };

    std::array<std::uint16_t, 36> indexes = {
        0, 1, 2,
        0, 2, 3,

        4, 5, 6,
        4, 7, 6,

        4, 5, 1,
        4, 1, 0,

        3, 2, 6,
        3, 6, 7,

        1, 5, 6,
        1, 6, 2,

        4, 0, 3,
        4, 3, 7
    };

    const UINT vbByteSize = (UINT)vertexes.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indexes.size() * sizeof(std::uint16_t);

    _pBoxGeometry = std::make_unique<MeshGeometry>();
    _pBoxGeometry->Name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, _pBoxGeometry->VertexBufferCPU.GetAddressOf()));
    CopyMemory(_pBoxGeometry->VertexBufferCPU->GetBufferPointer(), vertexes.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, _pBoxGeometry->IndexBufferCPU.GetAddressOf()));
    CopyMemory(_pBoxGeometry->IndexBufferCPU->GetBufferPointer(), indexes.data(), ibByteSize);

    _pBoxGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(_pDevice.Get(), _pCommandList.Get(), vertexes.data(), vbByteSize, _pBoxGeometry->VertexBufferUploader);
    _pBoxGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(_pDevice.Get(), _pCommandList.Get(), indexes.data(), ibByteSize, _pBoxGeometry->IndexBufferUploader);

    _pBoxGeometry->VertexByteStride = sizeof(Vertex);
    _pBoxGeometry->VertexBufferByteSize = vbByteSize;
    _pBoxGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
    _pBoxGeometry->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indexes.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    _pBoxGeometry->DrawArgs["box"] = submesh;
}

void BoxApp::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { _inputLayout.data(), (UINT)_inputLayout.size() };
    psoDesc.pRootSignature = _pRootSignature.Get();
    psoDesc.VS = {
        reinterpret_cast<BYTE*>(_pVSByteCode->GetBufferSize()),
        _pVSByteCode->GetBufferSize()
    };
    psoDesc.PS = {
        reinterpret_cast<BYTE*>(_pPSByteCode->GetBufferSize()),
        _pPSByteCode->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = _backBufferFormat;
    psoDesc.SampleDesc.Count = _b4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = _b4xMsaaState ? (_n4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = _depthStencilFormat;

    ThrowIfFailed(_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pPSO)));
}

