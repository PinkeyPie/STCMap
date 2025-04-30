#include "InitAppWindow.h"

using Microsoft::WRL::ComPtr;

InitAppWindow::InitAppWindow() : D3DApp() {}

InitAppWindow::~InitAppWindow() = default;

bool InitAppWindow::Initialize() {
    if(not D3DApp::Initialize()) {
        return false;
    }
    return true;
}

void InitAppWindow::OnResize() {
    D3DApp::OnResize();
}

void InitAppWindow::Update(const GameTimer &gt) {}

void InitAppWindow::Draw(const GameTimer &gt) {
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU
    ThrowIfFailed(_pDirectCommandListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList
    // Reusing the command list reuses memory
    ThrowIfFailed(_pCommandList->Reset(_pDirectCommandListAlloc.Get(), nullptr));

    // Indicate a state translation on resource usage
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    _pCommandList->ResourceBarrier(1, &barrier);

    // Set the viewport ans scissor rect. This needs to be reset whenever the command list is reset.
    _pCommandList->RSSetViewports(1, &_screenViewport);
    _pCommandList->RSSetScissorRects(1, &_scissorRect);

    // Clear the buffers
    _pCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    _pCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferDesc = CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = DepthStencilView();
    _pCommandList->OMSetRenderTargets(1, &backBufferDesc, true, &depthStencilView);

    // Indicate a state transition on the resource usage
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    _pCommandList->ResourceBarrier(1, &barrier);

    // Done recording commands
    ThrowIfFailed(_pCommandList->Close());

    // Add the command list to the queue for execution
    ID3D12CommandList* cmdLists[] = { _pCommandList.Get() };
    _pCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);


    // swap the back and from buffers
    ThrowIfFailed(_pSwapChain->Present(0, 0));
    _nCurrBackBuffer = (_nCurrBackBuffer + 1) % SwapChainBufferCount;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}


