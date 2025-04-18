#pragma once

#include <d2d1.h>
#include <windows.h>
#include "string"
#include "BaseWindow.h"
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <dxgi.h>
#include <dxcore.h>

class Window  : public BaseWindow {
public:
    Window() : BaseWindow(), pFactory(nullptr), pRenderTarget(nullptr), pBrush(nullptr) {}
    ~Window() override = default;
    [[nodiscard]] PCTCH ClassName() const override;
    std::wstring OpenFileDialog();
protected:
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
private:
    CComPtr<ID2D1Factory> pFactory;
    CComPtr<ID2D1HwndRenderTarget> pRenderTarget;
    CComPtr<ID2D1SolidColorBrush> pBrush;
    CComPtr<ID2D1SolidColorBrush> pStrokeBrush;
    D2D1_ELLIPSE ellipse;

    void DiscardGraphicsResources();
    void CalculateLayout();
    HRESULT CreateGraphicsResources();
    void OnPaint();
    void Resize();
    void DrawClockHand(float fHandLength, float fAngle, float fStrokeWidth);
};
