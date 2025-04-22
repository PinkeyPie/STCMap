#include "d3dApp.h"
#include <windows.h>
#include <windowsx.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

D3DApp* D3DApp::instance = nullptr;
D3DApp *D3DApp::GetApp() {
    return instance;
}

D3DApp::D3DApp(HINSTANCE hInstance) : BaseWindow() {
    if(hInstance == nullptr) {
        hInstance = GetModuleHandle(nullptr);
    }
    hAppInstance = hInstance;
    instance = this;
}

D3DApp::~D3DApp() {
    if(_pDevice != nullptr) {
        FlushCommandQueue();
    }
}

HINSTANCE D3DApp::AppInst() const {
    return hAppInstance;
}

PCTCH D3DApp::ClassName() const {
    return TEXT("MainWindow");
}

HWND D3DApp::MainWnd() const {
    return hwnd;
}

float D3DApp::AspectRatio() const {
    return static_cast<float>(_nClientWidth) / _nClientHeight;
}

bool D3DApp::Get4xMsaaState() const {
    return _b4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value) {
    if(_b4xMsaaState != value) {
        _b4xMsaaState = value;

        // Recreate the swap chain and buffers with new multisample settings
        CreateSwapChain();
        OnResize();
    }
}

int D3DApp::Run() {
    MSG msg = {};
    _timer.Reset();

    while (msg.message != WM_QUIT) {
        // If there are Window messages then process them.
        if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // Otherwise, do animation/game stuff.
        else {
            _timer.Tick();
            if(not _bAppPaused) {
                CalculateFrameStats();
                Update(_timer);
                Draw(_timer);
            }
            else {
                Sleep(100);
            }
        }
    }

    return static_cast<int>(msg.wParam);
}

bool D3DApp::Initialize() {
    if(not Create()) {
        return false;
    }
    if(not InitDirect3D()) {
        return false;
    }

    // Do initial resize code
    OnResize();

    return true;
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(_pRtvDescHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_pDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_pDsvDescHeap.GetAddressOf())));
}

void D3DApp::OnResize() {
    assert(_pDevice == nullptr);
    assert(_pSwapChain == nullptr);
    assert(_pDirectCommandListAlloc == nullptr);

    // Flush before changing any resources
    FlushCommandQueue();

    ThrowIfFailed(_pCommandList->Reset(_pDirectCommandListAlloc.Get(), nullptr));

    // Release the previous resources we will be recreating
    for(auto & buffer : _pSwapChainBuffer) {
        buffer.Reset();
    }
    _pDepthStencilBuffer.Reset();

    // Resize the swap chin buffers
    ThrowIfFailed(_pSwapChain->ResizeBuffers(
        SwapChainBufferCount,
        _nClientWidth, _nClientHeight,
        _backBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
    ));
    _nCurrBackBuffer = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart());
    for(UINT i = 0; i < SwapChainBufferCount; i++) {
        ThrowIfFailed(_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&_pSwapChainBuffer[i])));
        _pDevice->CreateRenderTargetView(_pSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, _nRtvDescriptorSize);
    }

    // Create the depth/stencil buffer and view
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = _nClientWidth;
    depthStencilDesc.Height = _nClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;

    // Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from
    // the depth buffer.  Therefore, because we need to create two views to the same resource:
    //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
    // we need to create the depth buffer resource with a typeless format.
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depthStencilDesc.SampleDesc.Count = _b4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = _b4xMsaaState ? (_n4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = _depthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(_pDevice->CreateCommittedResource(&heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(_pDepthStencilBuffer.GetAddressOf())));

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = _depthStencilFormat;
    dsvDesc.Texture2D.MipSlice = 0;
    _pDevice->CreateDepthStencilView(_pDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

    // Transition the resource from its initial state to be used as a depth buffer
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_pDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    _pCommandList->ResourceBarrier(1, &barrier);

    // Execute the resize command
    ThrowIfFailed(_pCommandList->Close());
    ID3D12CommandList* cmdsList[] = {_pCommandList.Get()};
    _pCommandQueue->ExecuteCommandLists(_countof(cmdsList), cmdsList);

    // Wait until resize is complete
    FlushCommandQueue();

    // Update the viewport transform to cover the client area
    _screenViewport.TopLeftX = 0;
    _screenViewport.TopLeftY = 0;
    _screenViewport.Height = static_cast<float>(_nClientHeight);
    _screenViewport.Width = static_cast<float>(_nClientWidth);
    _screenViewport.MinDepth = 0.f;
    _screenViewport.MaxDepth = 1.f;

    _scissorRect = {0, 0, _nClientWidth, _nClientHeight };
}

LRESULT D3DApp::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        // WM_ACTIVATE is sent when the window is activated or deactivated.
        // We pause the game when the window is deactivated and unpause it
        // when it becomes active.
        case WM_ACTIVATE:
            if(LOWORD(wParam) == WA_INACTIVE) {
                _bAppPaused = true;
                _timer.Stop();
            }
            else {
                _bAppPaused = false;
                _timer.Start();
            }
            return 0;
        // WM_SIZE is sent when the user resizes the window.
        case WM_SIZE:
            // Save the new client area dimensions.
            _nClientWidth = LOWORD(lParam);
            _nClientHeight = HIWORD(lParam);
            if(_pDevice) {
                if(wParam == SIZE_MINIMIZED) {
                    _bAppPaused = true;
                    _bMinimised = true;
                    _bMaximized = false;
                }
                else if(wParam == SIZE_MAXIMIZED) {
                    _bAppPaused = false;
                    _bMinimised = false;
                    _bMaximized = true;
                    OnResize();
                }
                else if(wParam == SIZE_RESTORED) {
                    // Restoring from minimized state?
                    if(_bMinimised) {
                        _bAppPaused = false;
                        _bMinimised = false;
                        OnResize();
                    }
                    // Restoring from maximized state?
                    else if(_bMaximized) {
                        _bAppPaused = false;
                        _bMaximized = false;
                        OnResize();
                    }
                    else if(_bResizing) {
                        // If user is dragging the resize bars, we do not resize
                        // the buffers here because as the user continuously
                        // drags the resize bars, a stream of WM_SIZE messages are
                        // sent to the window, and it would be pointless (and slow)
                        // to resize for each WM_SIZE message received from dragging
                        // the resize bars.  So instead, we reset after the user is
                        // done resizing the window and releases the resize bars, which
                        // sends a WM_EXITSIZEMOVE message.
                    }
                    else {
                        OnResize();
                    }
                }
            }
            return 0;
        // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
        case WM_ENTERSIZEMOVE:
            _bAppPaused = true;
            _bResizing = true;
            _timer.Stop();
            return 0;
        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
        case WM_EXITSIZEMOVE:
            _bAppPaused = false;
            _bResizing = false;
            _timer.Start();
            OnResize();
            return 0;
        case WM_KEYUP:
            if(wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            else if(static_cast<int>(wParam) == VK_F2) {
                Set4xMsaaState(!_b4xMsaaState);
            }
            return 0;
        default:
            break;
    }

    return BaseWindow::HandleMessage(uMsg, wParam, lParam);
}

bool D3DApp::InitDirect3D() {
#if defined(DEBUG) || defined(_DEBUG)
    // Enable the D3D12 debug layer
    {
        ComPtr<ID3D12Debug> debugController;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())));
        debugController->EnableDebugLayer();
    }
#endif
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(_pDxgiFactory.GetAddressOf())));
    // Try to create hardware device.
    HRESULT hr = D3D12CreateDevice(
        nullptr, // default adapter
        D3D_FEATURE_LEVEL_12_2,
        IID_PPV_ARGS(_pDevice.GetAddressOf()));

    // Fallback to WARP device
    if(FAILED(hr)) {
        ComPtr<IDXGIAdapter> pWarpAdapter;
        ThrowIfFailed(_pDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(pWarpAdapter.GetAddressOf())));
        ThrowIfFailed(D3D12CreateDevice(
            pWarpAdapter.Get(),
            D3D_FEATURE_LEVEL_12_2,
            IID_PPV_ARGS(_pDevice.GetAddressOf())));
    }

    ThrowIfFailed(_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_pFence.GetAddressOf())));

    _nRtvDescriptorSize = _pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    _nDsvDescriptorSize = _pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    _nCbvSrvUavDescriptorSize = _pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Check 4X MSAA quality support for our back buffer format.
    // All Direct3D 11 capable devices support 4X MSAA for all render
    // target formats, so we only need to check quality support.
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = _backBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    ThrowIfFailed(_pDevice->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &msQualityLevels,
        sizeof(msQualityLevels)));

    _n4xMsaaQuality = msQualityLevels.NumQualityLevels;
    assert(_n4xMsaaQuality > 0 and "Unexpected MSAA quality level.");

#ifdef DEBUG
    LogVideoAdapters();
#endif

    CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();

    return true;
}

void D3DApp::CreateCommandObjects() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(_pCommandQueue.GetAddressOf())));

    ThrowIfFailed(_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_pDirectCommandListAlloc.GetAddressOf())));
    ThrowIfFailed(_pDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        _pDirectCommandListAlloc.Get(), // Associated command allocator
        nullptr,                                // Initial PipelineStateObject
        IID_PPV_ARGS(_pCommandList.GetAddressOf())));

    // Start off in a closed state.  This is because the first time we refer
    // to the command list we will Reset it, and it needs to be closed before
    // calling Reset.
    _pCommandList->Close();
}

void D3DApp::CreateSwapChain() {
    // Release the previous swap chain we will be rendering
    _pSwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Width = _nClientWidth;
    swapChainDesc.BufferDesc.Height = _nClientHeight;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = _backBufferFormat;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = _b4xMsaaState ? 4 : 1;
    swapChainDesc.SampleDesc.Quality = _b4xMsaaState ? (_n4xMsaaQuality - 1) : 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SwapChainBufferCount;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Note: Swap chain uses queue to perform flush
    ThrowIfFailed(_pDxgiFactory->CreateSwapChain(
        _pCommandQueue.Get(),
        &swapChainDesc,
        _pSwapChain.GetAddressOf()));
}

void D3DApp::FlushCommandQueue() {
    // Advance the fence value to mark command up to this fence point
    _nCurrentFence++;

    // Add an instruction to the command queue to set a new fence point.  Because we
    // are on the GPU timeline, the new fence point won't be set until the GPU finishes
    // processing all the commands prior to this Signal().
    ThrowIfFailed(_pCommandQueue->Signal(_pFence.Get(), _nCurrentFence));

    // Wait until GPU finishes commands up to this fence point
    if(_pFence->GetCompletedValue() < _nCurrentFence) {
        HANDLE eventHandle = CreateEventEx(nullptr, TEXT(""), 0, EVENT_ALL_ACCESS);

        // Fire event when GPU hits current fence
        ThrowIfFailed(_pFence->SetEventOnCompletion(_nCurrentFence, eventHandle));

        // Wait until GPU sends event
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

ID3D12Resource *D3DApp::CurrentBackBuffer() const {
    return _pSwapChainBuffer[_nCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        _pRtvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        _nCurrBackBuffer,
        _nRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const {
    return _pDsvDescHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::CalculateFrameStats() {
    // Code computes the average frames per second, and also the
    // average time it takes to render one frame.  These stats
    // are appended to the window caption bar.

    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    // Compute averages over one second period
    if((_timer.TotalTime() - timeElapsed) >= 1.0f) {
        float fps = static_cast<float>(frameCnt);
        float spf = 1000.f / fps;

#ifdef _UNICODE
        wstring fpsStr = to_wstring(fps);
        wstring spfStr = to_wstring(spf);
        wstring windowText = wstring{ClassName()} + L" fps: " + fpsStr + L" spf: " + spfStr;
#else
        string fpsStr = to_string(fps);
        string spfStr = to_string(spf);
        string windowText = string{ClassName()} + " fps: " + fpsStr + " spf: " + spfStr;
#endif
        SetWindowText(hwnd, windowText.c_str());

        // Reset for next average
        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}

void D3DApp::LogVideoAdapters() {
    UINT i = 0;
    IDXGIAdapter* adapter = nullptr;
    vector<IDXGIAdapter*> adapterList;
    while (_pDxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";

        OutputDebugStringW(text.c_str());
        adapterList.push_back(adapter);

        ++i;
    }

    for (auto & a : adapterList) {
        LogVideoAdapterOutputs(a);
        ReleaseCom(a);
    }
}

void D3DApp::LogVideoAdapterOutputs(IDXGIAdapter *adapter) {
    UINT i = 0;
    IDXGIOutput* output = nullptr;
    while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND) {
        DXGI_OUTPUT_DESC outputDesc;
        output->GetDesc(&outputDesc);

        wstring text = L"***Output";
        text += outputDesc.DeviceName;
        text += L"\n";
        OutputDebugStringW(text.c_str());
        LogOutputDisplayModes(output, _backBufferFormat);
        ReleaseCom(output);
        ++i;
    }
}

void D3DApp::LogOutputDisplayModes(IDXGIOutput *output, DXGI_FORMAT format) {
    UINT count = 0;
    UINT flags = 0;

    // Call with nullptr to get list count.
    output->GetDisplayModeList(format, flags, &count, nullptr);

    vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flags, &count, &modeList[0]);

    for(auto& x : modeList) {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;
        wstring text = L"width: " + to_wstring(x.Width) + L" " +
                       L"height: " + to_wstring(x.Height) + L" " +
                       L"refresh: " + to_wstring(n) + L"/" + to_wstring(d) +
                       L"\n";

        OutputDebugStringW(text.c_str());
    }
}

