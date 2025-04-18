//
// Created by Chingis on 11.10.2024.
//

#include "Window.h"
#include <shobjidl_core.h>
#include "iostream"

PCTCH Window::ClassName() const {
    return TEXT("MainWindow");
}

void Window::CalculateLayout() {
    if(pRenderTarget != nullptr) {
        D2D1_SIZE_F size = pRenderTarget->GetSize();
        const float x = size.width / 2;
        const float y = size.height / 2;
        const float radius = min(x, y);
        ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius);
    }
}

HRESULT Window::CreateGraphicsResources() {
    if(pRenderTarget == nullptr) {
        RECT rc;
        if(GetClientRect(hwnd, &rc)) {
            const D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
            std::cout << "size = (" << size.width << ", " << size.height << ");" << std::endl;
            try {
                throw_if_fail(pFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd, size), &pRenderTarget));

                const D2D1_COLOR_F ellipseColor = D2D1::ColorF(1.0f, 0.0f, 1.0f);
                throw_if_fail(pRenderTarget->CreateSolidColorBrush(ellipseColor, &pBrush));
                const D2D1_COLOR_F strokeColor = D2D1::ColorF(0.f, 0.f, 0.f);
                throw_if_fail(pRenderTarget->CreateSolidColorBrush(strokeColor, &pStrokeBrush));
                CalculateLayout();
            }
            catch (_com_error& error) {
                std::wcout << "Error occurred during creating graphics resources error: " << error.ErrorMessage() << std::endl;
                return error.Error();
            }
        }
    }
    return S_OK;
}

void Window::DiscardGraphicsResources() {
    pRenderTarget.Release();
    pBrush.Release();
}

void Window::OnPaint() {
    try {
        throw_if_fail(CreateGraphicsResources());
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        pRenderTarget->BeginDraw();

        CComPtr<IDXGIFactory> pFactory;
        DXGI_SWAP_CHAIN_DESC swapChainDesc;

        D3D_FEATURE_LEVEL featureLevels[3] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3
        };

        D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelsInfo;
        featureLevelsInfo.NumFeatureLevels = 3;
        featureLevelsInfo.pFeatureLevelsRequested = featureLevels;



        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue));
        pRenderTarget->FillEllipse(ellipse, pBrush);
        pRenderTarget->DrawEllipse(ellipse, pStrokeBrush);
        SYSTEMTIME time;
        GetLocalTime(&time);
        pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        if(HRESULT hr = pRenderTarget->EndDraw(); FAILED(hr) or hr == D2DERR_RECREATE_TARGET) {
            DiscardGraphicsResources();
        }
        EndPaint(hwnd, &ps);
    }
    catch (_com_error& error) {
        DiscardGraphicsResources();
        throw std::runtime_error{"Error with graphics pipeline"};
    }
}

void Window::Resize() {
    if(pRenderTarget != nullptr) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        pRenderTarget->Resize(size);
        CalculateLayout();
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void Window::DrawClockHand(float fHandLength, float fAngle, float fStrokeWidth) {
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Rotation(fAngle, ellipse.point));
    D2D_POINT_2F endPoint = D2D1::Point2F(ellipse.point.x, ellipse.point.y - (ellipse.radiusY * fHandLength));
    pRenderTarget->DrawLine(ellipse.point, endPoint, pBrush, fStrokeWidth);
}

LRESULT Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory))) {
                return -1;
            }
            return 0;
        case WM_DESTROY:
            DiscardGraphicsResources();
            pFactory.Release();
            PostQuitMessage(0);
            return 0;
        case WM_PAINT:
            OnPaint();
            return 0;
        case WM_SIZE:
            Resize();
            return 0;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

std::wstring Window::OpenFileDialog() {
    std::wstring wszFileName;
    try {
        CComPtr<IFileOpenDialog> pFileOpen;
        throw_if_fail(pFileOpen.CoCreateInstance(__uuidof(FileOpenDialog)));
        throw_if_fail(pFileOpen->Show(nullptr));
        CComPtr<IShellItem> pItem;
        throw_if_fail(pFileOpen->GetResult(&pItem));
        PWSTR pszFilePath;
        throw_if_fail(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));
        wszFileName = std::wstring{pszFilePath};
        CoTaskMemFree(pszFilePath);
    }
    catch (_com_error& error) {
        wszFileName = L"";
    }
    return wszFileName;
}

