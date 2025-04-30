#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "BaseWindow.h"
#include "../utils/d3dUtil.h"
#include "../GameTimer.h"

class COMMON_EXPORT D3DApp : public BaseWindow {
protected:
    explicit D3DApp();
    ~D3DApp() override;
public:
    explicit D3DApp(const D3DApp& o) = delete;
    D3DApp& operator=(const D3DApp& o) = delete;
    static D3DApp* GetApp(); // singleton

    [[nodiscard]] HINSTANCE AppInst() const;
    [[nodiscard]] HWND MainWnd() const;
    [[nodiscard]] float AspectRatio() const;

    [[nodiscard]] bool Get4xMsaaState() const;
    void Set4xMsaaState(bool value);

    bool Initialize() override;
    int Run() override;

    [[nodiscard]] PCTCH ClassName() const override;
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

protected:
    virtual void CreateRtvAndDsvDescriptorHeaps();
    virtual void OnResize();
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;

protected:
    bool InitDirect3D();
    void CreateCommandObjects();
    void CreateSwapChain();

    void FlushCommandQueue();

    [[nodiscard]] ID3D12Resource* CurrentBackBuffer() const;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    void CalculateFrameStats();

    void LogVideoAdapters();
    void LogVideoAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:
    static D3DApp* instance;
    bool _bAppPaused = false;
    bool _bMinimised = false;
    bool _bMaximized = false;
    bool _bResizing = false;
    bool _bFullscreenState = false;

    bool _b4xMsaaState = false;
    UINT _n4xMsaaQuality = 0;

    GameTimer _timer;

    Microsoft::WRL::ComPtr<IDXGIFactory4> _pDxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain> _pSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device> _pDevice;

    Microsoft::WRL::ComPtr<ID3D12Fence> _pFence;
    UINT _nCurrentFence = 0;

    static constexpr int SwapChainBufferCount = 2;
    int _nCurrBackBuffer = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> _pSwapChainBuffer[SwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> _pDepthStencilBuffer;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _pRtvDescHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _pDsvDescHeap;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> _pCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _pCommandList;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _pDirectCommandListAlloc;

    D3D12_VIEWPORT _screenViewport;
    D3D12_RECT _scissorRect;

    UINT _nRtvDescriptorSize = 0;
    UINT _nDsvDescriptorSize = 0;
    UINT _nCbvSrvUavDescriptorSize = 0;

    D3D_DRIVER_TYPE _d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT _backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT _depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
};
