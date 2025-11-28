#pragma once

#include "pch.h"
#include "directx/dx.h"

class BaseWindow {
public:
    explicit BaseWindow() = default;
    virtual ~BaseWindow() = default;

    static LRESULT CALLBACK WindowsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    [[nodiscard]] HWND GetWindow() const { return hwnd; }
    virtual bool Initialize(const TCHAR* name, int initialWidth = CW_USEDEFAULT, int initialHeight = CW_USEDEFAULT, ColorDepth colorDepth = EColorDepth8);
    void SetTitle(const WCHAR* format, ...);

    int ClientWidth = 800;
    int ClientHeight = 600;

protected:
    BOOL Create(PCWCH lpWindowName,
        DWORD dwStyle,
        DWORD dwExStyle = 0,
        int x = CW_USEDEFAULT,
        int y = CW_USEDEFAULT,
        HWND hWndParent = nullptr,
        HMENU hMenu = nullptr);
    virtual PCWCH ClassName() const = 0;
    virtual void MouseDownHandle(WPARAM btnState, int x, int y) {}
    virtual void MouseUpHandle(WPARAM btnState, int x, int y) {}
    virtual void MouseMoveHandle(WPARAM btnState, int x, int y) {}

    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    HINSTANCE hAppInstance = nullptr;
    HWND hwnd = nullptr;
    float g_DPIScale = 1.0f;

    void InitializeDPIScale(HWND hwnd) {
        const auto dpi = static_cast<float>(GetDpiForWindow(hwnd));
        g_DPIScale = dpi / USER_DEFAULT_SCREEN_DPI;
    }
    template<typename T>
    float PixelsToDips(T x) {
        return static_cast<float>(x) / g_DPIScale;
    }
    template<class T>
    float PixelsToDipsY(T y) {
        return static_cast<float>(y) / g_DPIScale;
    }
};
