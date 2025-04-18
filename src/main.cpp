#include <iostream>
#include <windows.h>
#include "Windows/Window.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d12.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow) {
    try {
        throw_if_fail(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
    }
    catch (_com_error& error) {
        std::cout << "Exception appeared error string: " << std::endl;
        std::wcout << error.ErrorMessage() << std::endl;
        return -1;
    }
    Window window;
    if(!window.Create(TEXT("Learn to program windows"), WS_OVERLAPPEDWINDOW)) {
        return 0;
    }
    ShowWindow(window.GetWindow(), nCmdShow);
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
