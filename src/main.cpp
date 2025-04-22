#include <iostream>
#include <windows.h>
#include "window/BaseWindow.h"
#include "Windows/Window.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if(FAILED(hr)) {
        std::cout << "Exception appeared error string: " << std::endl;
        return -1;
    }
    Window window;
    if(!window.Create()) {
        return 0;
    }
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
