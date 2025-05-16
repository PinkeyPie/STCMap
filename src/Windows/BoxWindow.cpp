#include "BoxWindow.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

struct VPosData {
    DirectX::XMFLOAT3 Pos;
};

struct VColorData {
    DirectX::XMFLOAT4 Color;
};

struct ObjectConstants {
    DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4 PulseColor = { 1.f, 1.f, 1.f, 1.f };
    float Time = 0.f;
};

bool BoxWindow::Initialize() {
    if(not D3DApp::Initialize()) {
        return false;
    }
    /*float scissorWidth = _nClientWidth / 2;
    float scissorHeight = _nClientHeight / 2;
    _scissorRect.left = _screenViewport.Width / 2 - scissorWidth / 2;
    _scissorRect.right = _screenViewport.Width / 2 + scissorWidth / 2;
    _scissorRect.top = _screenViewport.Height / 2 - scissorHeight / 2;
    _scissorRect.bottom = _screenViewport.Height / 2 + scissorHeight / 2;*/

    // Reset the command list to prep for inititalization commands;
    ThrowIfFailed(_pCommandList->Reset(_pDirectCommandListAlloc.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();

    BuildBoxGeometry(XMFLOAT3{0, 0, 0});
    BuildPrismTopology(XMFLOAT3(4, 0, 4));
    BuildPyramidTopology(XMFLOAT3{ -4, 0, 4 });
    BuildComplexGeometry();    

    BuildPSO();
    // Execute the initialization commands
    ThrowIfFailed(_pCommandList->Close());
    ID3D12CommandList* cmdLists[] = { _pCommandList.Get() };
    _pCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // Wait until initialization finished
    FlushCommandQueue();

    return true;
}

void BoxWindow::ResizeHandle() {
    D3DApp::ResizeHandle();

    // The window resized, so update the aspect ratio and recompile the projection matrix
    XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.f, 1000.f);
    XMStoreFloat4x4(&_proj, proj);
}

void BoxWindow::Update(const GameTimer &gt) {
    // Convert Spherical to Cartesian coordinates
    float x = _fRadius * sinf(_fPhi) * cosf(_fTheta);
    float z = _fRadius * sinf(_fPhi) * sinf(_fTheta);
    float y = _fRadius * cosf(_fPhi);

    // Build the view matrix
    XMVECTOR pos = XMVectorSet(x, y, z, 1.f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    XMMATRIX world = XMLoadFloat4x4(&_world);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&_view, view);    
    XMMATRIX proj = XMLoadFloat4x4(&_proj);

    XMMATRIX worldViewProj = world * view * proj;
    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    objConstants.Time = gt.TotalTime();
    objConstants.PulseColor = XMFLOAT4(Colors::Red);
    _pObjectCB->CopyData(0, objConstants);
}

void BoxWindow::Draw(const GameTimer &gt) {
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

    _pCommandList->SetGraphicsRootSignature(_pRootSignature.Get());
    
    ID3D12DescriptorHeap* descriptorHeaps[] = { _pCbvHeap.Get() };
    _pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);    

    // const D3D12_VERTEX_BUFFER_VIEW vertexes[] = { _pBoxPositionGeometry->VertexBufferView(), _pBoxColorGeometry->VertexBufferView() };
    _pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D12_VERTEX_BUFFER_VIEW vertexes = _pGeometry->VertexBufferView();
    const D3D12_INDEX_BUFFER_VIEW indexes = _pGeometry->IndexBufferView();
    _pCommandList->IASetVertexBuffers(0, 1, &vertexes);
    _pCommandList->IASetIndexBuffer(&indexes);
    _pCommandList->SetGraphicsRootDescriptorTable(0, _pCbvHeap->GetGPUDescriptorHandleForHeapStart());

    UINT baseIndexLocation = 0;
    UINT baseVertexLocation = 0;
    UINT instance = 0;
    for (auto& geometry : _pGeometry->DrawArgs) {
        _pCommandList->DrawIndexedInstanced(geometry.second.IndexCount, 1, geometry.second.StartIndexLocation, geometry.second.BaseVertexLocation, instance++);
    }

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

void BoxWindow::MouseDawnHandle(WPARAM btnState, int x, int y) {
    _lastMousePos.x = x;
    _lastMousePos.y = y;

    SetCapture(hwnd);
}

void BoxWindow::MouseUpHandle(WPARAM btnState, int x, int y) {
    ReleaseCapture();
}

void BoxWindow::MouseMoveHandle(WPARAM btnState, int x, int y) {
    if((btnState & MK_LBUTTON) != 0) {
        // Make each pixel correspond to a quarter of degree
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - _lastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - _lastMousePos.y));

        // Update angles based on input to orbit camera around box
        _fTheta -= dx;
        _fPhi -= dy;

        // Restrict the angle phi
        _fPhi = MathHelper::Clamp(_fPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0) {
        // Make each pixel correspond to 0.005 unit in the scene
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

void BoxWindow::BuildDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_pDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(_pCbvHeap.GetAddressOf())));
}

void BoxWindow::BuildConstantBuffers() {
    _pObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(_pDevice.Get(), 1, true);
    std::vector<D3D12_CONSTANT_BUFFER_VIEW_DESC> objectsCbvDesc(1);

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = _pObjectCB->Resource()->GetGPUVirtualAddress();
    
    D3D12_CONSTANT_BUFFER_VIEW_DESC objDesc;
    objDesc.BufferLocation = cbAddress;
    objDesc.SizeInBytes = objCBByteSize;

    _pDevice->CreateConstantBufferView(&objDesc, _pCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxWindow::BuildRootSignature() {
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

    // create a root signature with a single slot which points to a descriptor range consisting of a multiple constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if(errorBlob != nullptr) {
        OutputDebugString((TCHAR*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(_pDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_pRootSignature.GetAddressOf())));
}

void BoxWindow::BuildShadersAndInputLayout() {
    TCHAR AbsolutePath[MAX_PATH];
    TCHAR ShaderPath[] = TEXT("\\shaders\\color.hlsl");
    DWORD dwRet = GetCurrentDirectory(MAX_PATH, AbsolutePath);
    tcscat(AbsolutePath, ShaderPath);

    _pVSByteCode = d3dUtil::CompileShader(AbsolutePath, nullptr, "VS", "vs_5_0");
    _pPSByteCode = d3dUtil::CompileShader(AbsolutePath, nullptr, "PS", "ps_5_0");

    _inputLayout = {
        { "COLOR",0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void GetBoxVertexesAndIndexes(std::vector<Vertex> &vertices, std::vector<std::uint16_t> &indexes) {
    vertices = {
        Vertex{XMFLOAT3(-1.f,-1.f,-1.f), XMFLOAT4(Colors::White)},
        Vertex{XMFLOAT3(-1.f, +1.f, -1.f), XMFLOAT4(Colors::Black)},
        Vertex{XMFLOAT3(+1.f, +1.f, -1.f), XMFLOAT4(Colors::Red)},
        Vertex{XMFLOAT3(+1.f, -1.f, -1.f), XMFLOAT4(Colors::Green)},
        Vertex{XMFLOAT3(-1.f, -1.f, +1.f), XMFLOAT4(Colors::Blue)},
        Vertex{XMFLOAT3(-1.f, +1.f, +1.f), XMFLOAT4(Colors::Yellow)},
        Vertex{XMFLOAT3(+1.f, +1.f, +1.f), XMFLOAT4(Colors::Cyan)},
        Vertex{XMFLOAT3(+1.f, -1.f, +1.f), XMFLOAT4(Colors::Magenta)}
    };
    indexes = {
        0, 1, 2,
        0, 2, 3,

        4, 6, 5,
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
}

void GetPyramidVertexesAndIndexes(std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indexes) {
    vertices = {
        Vertex{XMFLOAT3(+1.f, -1.f, -1.f), XMFLOAT4(Colors::Green)},
        Vertex{XMFLOAT3(-1.f, -1.f, -1.f), XMFLOAT4(Colors::Green)},
        Vertex{XMFLOAT3(-1.f, -1.f, +1.f), XMFLOAT4(Colors::Green)},
        Vertex{XMFLOAT3(+1.f, -1.f, +1.f), XMFLOAT4(Colors::Green)},
        Vertex{XMFLOAT3(0.f, +1.f, 0.f), XMFLOAT4(Colors::Red)}
    };

    indexes = {
        // front
        1, 4, 0,

        // back
        3, 4, 2,

        // left
        2, 4, 1,

        // right
        0, 4, 3,

        // bottom
        2, 1, 0,
        2, 0, 3
    };
}

void GetPrismVertexesAndIndexes(std::vector<Vertex>& vertexes, std::vector<std::uint16_t>& indexes) {
    vertexes = {
        Vertex{XMFLOAT3(+1.f, -1.f, -1.f), XMFLOAT4(Colors::Green)},
        Vertex{XMFLOAT3(-1.f, -1.f, -1.f), XMFLOAT4(Colors::Red)},
        Vertex{XMFLOAT3(-1.f, +1.f, -1.f), XMFLOAT4(Colors::Blue)},
        Vertex{XMFLOAT3(+1.f, +1.f, -1.f), XMFLOAT4(Colors::White)},
        Vertex(XMFLOAT3(0.f, +1.f, +1.f), XMFLOAT4(Colors::Magenta)),
        Vertex(XMFLOAT3(0.f, -1.f, +1.f), XMFLOAT4(Colors::Yellow))
    };

    indexes = {
        1, 2, 3,
        1, 3, 0,

        5, 4, 2,
        5, 2, 1,

        0, 3, 4,
        0, 4, 5,

        2, 4, 3,

        1, 0, 5
    };
}

void BoxWindow::BuildComplexGeometry() {
    _pGeometry = std::make_unique<MeshGeometry>();
    _pGeometry->Name = "complexGeometry";

    std::vector<Vertex> vertexes;
    std::vector<std::uint16_t> indexes;
    int vertexOffset = 0;
    int indexOffset = 0;

    for (int i = 0; i < _geometryTypes.size(); i++) {
        std::vector<Vertex> geometryVertexes;
        std::vector<uint16_t> geometryIndexes;
        
        EGeometryType type = _geometryTypes[i];
        XMFLOAT3 position = _geometryPositions[i];
        XMMATRIX world = XMMatrixSet(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            position.x, position.y, position.z, 1
        );
        switch (type) {
        case E_BOX:
            GetBoxVertexesAndIndexes(geometryVertexes, geometryIndexes);
            break;
        case E_PYRAMID:
            GetPyramidVertexesAndIndexes(geometryVertexes, geometryIndexes);
            break;
        case E_PRISM:
            GetPrismVertexesAndIndexes(geometryVertexes, geometryIndexes);
            break;
        default:
            break;
        }        
        for (int j = 0; j < geometryVertexes.size(); j++) {
            XMVECTOR posVec = XMVectorSet(geometryVertexes[j].Pos.x, geometryVertexes[j].Pos.y, geometryVertexes[j].Pos.z, 1);
            XMVECTOR worldPos = XMVector4Transform(posVec, world);
            XMStoreFloat3(&geometryVertexes[j].Pos, worldPos);
        }
        vertexes.insert(vertexes.end(), geometryVertexes.begin(), geometryVertexes.end());
        indexes.insert(indexes.end(), geometryIndexes.begin(), geometryIndexes.end());

        SubmeshGeometry submesh;
        submesh.IndexCount = (UINT)geometryIndexes.size();
        submesh.StartIndexLocation = indexOffset;
        submesh.BaseVertexLocation = vertexOffset;
        std::string submeshName = "geometry" + std::to_string(i);
        _pGeometry->DrawArgs[submeshName] = submesh;
        vertexOffset = vertexes.size();
        indexOffset = indexes.size();
    }

    const UINT vbByteSize = (UINT)vertexes.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indexes.size() * sizeof(std::uint16_t);

    ThrowIfFailed(D3DCreateBlob(vbByteSize, _pGeometry->VertexBufferCPU.GetAddressOf()));
    CopyMemory(_pGeometry->VertexBufferCPU->GetBufferPointer(), vertexes.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, _pGeometry->IndexBufferCPU.GetAddressOf()));
    CopyMemory(_pGeometry->IndexBufferCPU->GetBufferPointer(), indexes.data(), ibByteSize);

    _pGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(_pDevice.Get(), _pCommandList.Get(), vertexes.data(), vbByteSize, _pGeometry->VertexBufferUploader);
    _pGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(_pDevice.Get(), _pCommandList.Get(), indexes.data(), ibByteSize, _pGeometry->IndexBufferUploader);

    _pGeometry->VertexByteStride = sizeof(Vertex);
    _pGeometry->VertexBufferByteSize = vbByteSize;
    _pGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
    _pGeometry->IndexBufferByteSize = ibByteSize;
}

void BoxWindow::BuildPyramidTopology(XMFLOAT3 Position) {
    _geometryTypes.push_back(E_PYRAMID);
    _geometryPositions.push_back(Position);
}

void BoxWindow::BuildPrismTopology(XMFLOAT3 Position) {
    _geometryTypes.push_back(E_PRISM);
    _geometryPositions.push_back(Position);
}

void BoxWindow::BuildBoxGeometry(XMFLOAT3 Position) {
    /* std::array<VPosData, 8> vertexPositions = {
        XMFLOAT3(-1.f,-1.f,-1.f),
        XMFLOAT3(-1.f, +1.f, -1.f),
        XMFLOAT3(+1.f, +1.f, -1.f),
        XMFLOAT3(+1.f, -1.f, -1.f),
        XMFLOAT3(-1.f, -1.f, +1.f),
        XMFLOAT3(-1.f, +1.f, +1.f),
        XMFLOAT3(+1.f, +1.f, +1.f),
        XMFLOAT3(+1.f, -1.f, +1.f)
    };
    std::array<VColorData, 8> vertexColors = {
        XMFLOAT4(Colors::White),
        XMFLOAT4(Colors::Black),
        XMFLOAT4(Colors::Red),
        XMFLOAT4(Colors::Green),
        XMFLOAT4(Colors::Blue),
        XMFLOAT4(Colors::Yellow),
        XMFLOAT4(Colors::Cyan),
        XMFLOAT4(Colors::Magenta)
    };

    std::array<std::uint16_t, 36> indexes = {
        0, 1, 2,
        0, 2, 3,

        4, 6, 5,
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

    const UINT vpbByteSize = (UINT)vertexPositions.size() * sizeof(VPosData);
    const UINT vcbByteSize = (UINT)vertexColors.size() * sizeof(VColorData);
    const UINT ibByteSize = (UINT)indexes.size() * sizeof(std::uint16_t);

    _pBoxPositionGeometry = std::make_unique<MeshGeometry>();
    _pBoxPositionGeometry->Name = "boxPositionGeo";

    ThrowIfFailed(D3DCreateBlob(vpbByteSize, _pBoxPositionGeometry->VertexBufferCPU.GetAddressOf()));
    CopyMemory(_pBoxPositionGeometry->VertexBufferCPU->GetBufferPointer(), vertexPositions.data(), vpbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, _pBoxPositionGeometry->IndexBufferCPU.GetAddressOf()));
    CopyMemory(_pBoxPositionGeometry->IndexBufferCPU->GetBufferPointer(), indexes.data(), ibByteSize);

    _pBoxPositionGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(_pDevice.Get(), _pCommandList.Get(), vertexPositions.data(), vpbByteSize, _pBoxPositionGeometry->VertexBufferUploader);
    _pBoxPositionGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(_pDevice.Get(), _pCommandList.Get(), indexes.data(), ibByteSize, _pBoxPositionGeometry->IndexBufferUploader);

    _pBoxPositionGeometry->VertexByteStride = sizeof(VPosData);
    _pBoxPositionGeometry->VertexBufferByteSize = vpbByteSize;
    _pBoxPositionGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
    _pBoxPositionGeometry->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indexes.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    _pBoxPositionGeometry->DrawArgs["box"] = submesh;

    _pBoxColorGeometry = std::make_unique<MeshGeometry>();
    _pBoxColorGeometry->Name = "boxColorGeo";
    ThrowIfFailed(D3DCreateBlob(vcbByteSize, _pBoxColorGeometry->VertexBufferCPU.GetAddressOf()));
    CopyMemory(_pBoxColorGeometry->VertexBufferCPU->GetBufferPointer(), vertexColors.data(), vcbByteSize);
    _pBoxColorGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(_pDevice.Get(), _pCommandList.Get(), vertexColors.data(), vcbByteSize, _pBoxColorGeometry->VertexBufferUploader);

    _pBoxColorGeometry->VertexByteStride = sizeof(VColorData);
    _pBoxColorGeometry->VertexBufferByteSize = vcbByteSize;

    _pBoxColorGeometry->DrawArgs["box"] = submesh; */
    _geometryTypes.push_back(E_BOX);
    _geometryPositions.push_back(Position);
}

void BoxWindow::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { _inputLayout.data(), (UINT)_inputLayout.size() };
    psoDesc.pRootSignature = _pRootSignature.Get();
    psoDesc.VS = {
        reinterpret_cast<BYTE*>(_pVSByteCode->GetBufferPointer()),
        _pVSByteCode->GetBufferSize()
    };
    psoDesc.PS = {
        reinterpret_cast<BYTE*>(_pPSByteCode->GetBufferPointer()),
        _pPSByteCode->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    /*psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;*/
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