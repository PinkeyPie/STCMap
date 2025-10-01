#include "BaseWindow.h"

LRESULT BaseWindow::WindowsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	BaseWindow* pThis = nullptr;
	if (uMsg == WM_NCCREATE) {
		const auto pCreate = (CREATESTRUCT*)(lParam);
		pThis = (BaseWindow*)(pCreate->lpCreateParams);
		SetWindowLong(hwnd, GWLP_USERDATA, (LONG_PTR)(pThis));
		pThis->hwnd = hwnd;
	}
	else {
		pThis = (BaseWindow*)(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}
	if (pThis) {
		return pThis->HandleMessage(uMsg, wParam, lParam);
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL BaseWindow::Create(PCTCH lpWindowName, DWORD dwStyle, DWORD dwExStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu) {
	hAppInstance = GetModuleHandle(nullptr);
	if (GetClassInfo(hAppInstance, ClassName(), nullptr)) {
		return FALSE;
	}
	WNDCLASS wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpszClassName = ClassName();
	wc.lpfnWndProc = WindowsProc;
	wc.hInstance = hAppInstance;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = ClassName();
	if (!RegisterClass(&wc)) {
		MessageBox(0, "RegisterClass failed.", 0, 0);
		return FALSE;
	}

	RECT R = { 0, 0, ClientWidth, ClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	hwnd = CreateWindowEx(dwExStyle, ClassName(), lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hAppInstance, this);
	if (!hwnd) {
		MessageBox(nullptr, TEXT("CreateWindow Failed."), nullptr, 0);
	}
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	return TRUE;
}

LRESULT BaseWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
		// The WM_MENUCHAR message is sent when a menu is active and the user presses
		// a key that does not correspond to any mnemonic or accelerator key.
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);
		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		MouseDownHandle(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		MouseUpHandle(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		MouseMoveHandle(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	default:
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool BaseWindow::Initialize() {
	return Create(TEXT("window"), WS_OVERLAPPEDWINDOW);
}
