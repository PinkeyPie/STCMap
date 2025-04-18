//
// Created by Chingis on 11.10.2024.
//

#include "BaseWindow.h"

#include <d3d12.h>
#include <dxgi.h>

#include "iostream"
#include <vector>
#include <wrl.h>

LRESULT BaseWindow::WindowsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    BaseWindow* pThis = nullptr;
    if(uMsg == WM_NCCREATE) {
        const auto pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = static_cast<BaseWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->hwnd = hwnd;
    }
    else {
        pThis = reinterpret_cast<BaseWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if(pThis) {
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL BaseWindow::Create(PCTCH lpWindowName, DWORD dwStyle, DWORD dwExStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu) {
    WNDCLASS wc = {};
    wc.lpszClassName = ClassName();
    wc.lpfnWndProc = BaseWindow::WindowsProc;
    wc.hInstance = GetModuleHandle(nullptr);

    RegisterClass(&wc);
    hwnd = CreateWindowEx(dwExStyle, ClassName(), lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, GetModuleHandle(nullptr), this);
    return hwnd ? TRUE : FALSE;
}

void BaseWindow::LogAdapters() {
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    

    UINT i = 0;
    IDXGIAdapter* adapter = nullptr;
    std::vector<IDXGIAdapter*> adapterList;
    while (pDxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";

        OutputDebugStringW(text.c_str());
        adapterList.push_back(adapter);
        ++i;


    }

    adapter->Release();
}

