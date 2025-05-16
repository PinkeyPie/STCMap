#include <iostream>
#include <windows.h>

#include "Windows/BoxWindow.h"
#include "Windows/InitAppWindow.h"
#include "Windows/Window.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow) {
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    try {
        auto window = new BoxApp;
        if(!window->Initialize()) {
            std::cout << "Exception during window creation" << std::endl;
            return -1;
        }
        window->Run();
    }
    catch (DxException& e) {
        MessageBox(nullptr, e.ToString().c_str(), TEXT("HR Failed"), MB_OK);
    }
    return 0;
}
