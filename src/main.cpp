#include <iostream>

#include "Application.h"
#include "pch.h"
#include "directx/dx.h"
#include "directx/DxContext.h"
#include "core/memory.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow) {
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try {
		DxContext::Instance().Initialize();
		const auto application = Application::Instance();
		if (!application->Initialize()) {
			std::cout << "Exception during application creation\n";
			return -1;
		}
		application->Run();
	}
	catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
	}
	return 0;
}
