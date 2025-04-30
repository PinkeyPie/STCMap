#pragma once

#include <d2d1.h>
#include <windows.h>
#include "string"
#include "window/BaseWindow.h"
#include <wrl/client.h>

class Window : public BaseWindow {
public:
    Window() : BaseWindow(), pFactory(nullptr), pRenderTarget(nullptr), pBrush(nullptr) {}
    ~Window() override = default;
    [[nodiscard]] PCTCH ClassName() const override;
    std::wstring OpenFileDialog();
    bool Initialize() override;
protected:
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
private:
    Microsoft::WRL::ComPtr<ID2D1Factory> pFactory;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> pRenderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> pBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> pStrokeBrush;
    D2D1_ELLIPSE ellipse;

    void DiscardGraphicsResources();
    void CalculateLayout();
    HRESULT CreateGraphicsResources();
    void OnPaint();
    void Resize();
    void DrawClockHand(float fHandLength, float fAngle, float fStrokeWidth);
};
