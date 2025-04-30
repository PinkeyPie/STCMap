#pragma once

#include <dxgi.h>
#include "../CommonGlobal.h"

class COMMON_EXPORT BaseWindow {
public:
    explicit BaseWindow() = default;
    virtual ~BaseWindow() = default;

    static LRESULT CALLBACK WindowsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    [[nodiscard]] HWND GetWindow() const { return hwnd; }
    virtual bool Initialize();
    virtual int Run();
protected:
    BOOL Create(PCTCH lpWindowName,
        DWORD dwStyle,
        DWORD dwExStyle = 0,
        int x = CW_USEDEFAULT,
        int y = CW_USEDEFAULT,
        int nWidth = CW_USEDEFAULT,
        int nHeight = CW_USEDEFAULT,
        HWND hWndParent = nullptr,
        HMENU hMenu = nullptr);
    [[nodiscard]] virtual PCTCH ClassName() const = 0;
    virtual void MouseDawnHandle(WPARAM btnState, int x, int y) {}
    virtual void MouseUpHandle(WPARAM btnState, int x, int y) {}
    virtual void MouseMoveHandle(WPARAM btnState, int x, int y) {}

    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    HINSTANCE hAppInstance = nullptr;
    HWND hwnd = nullptr;
    int _nClientWidth = 800;
    int _nClientHeight = 600;
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
