#pragma once

#include <atlbase.h>
#include <comdef.h>
#include <dxgi.h>

inline void throw_if_fail(HRESULT hr) {
    if(FAILED(hr)) {
        throw _com_error(hr);
    }
}

class BaseWindow {
public:
    explicit BaseWindow() : hwnd(nullptr) {}
    virtual ~BaseWindow() = default;
    static LRESULT CALLBACK WindowsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL Create(PCTCH lpWindowName, DWORD dwStyle, DWORD dwExStyle = 0, int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
        int nWidth = CW_USEDEFAULT, int nHeight = CW_USEDEFAULT, HWND hWndParent = nullptr, HMENU hMenu = nullptr);
    [[nodiscard]] HWND GetWindow() const { return hwnd; }
protected:
    [[nodiscard]] virtual PCTCH ClassName() const = 0;
    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
    HWND hwnd;
    CComPtr<IDXGIFactory> pDxgiFactory;

    float g_DPIScale = 1.0f;
    void InitializeDPIScale(HWND hwnd) {
        float dpi = GetDpiForWindow(hwnd);
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

    void LogAdapters();
};
