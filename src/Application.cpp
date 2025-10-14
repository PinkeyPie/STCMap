#include "Application.h"

#include "directx/BarrierBatcher.h"
#include "directx/DxCommandList.h"
#include "directx/DxContext.h"
#include "directx/DxRenderer.h"

Application* Application::_instance = new Application{};

namespace {
	LONG NTAPI HandleVectoredException(PEXCEPTION_POINTERS exceptionInfo) {
		PEXCEPTION_RECORD exceptionRecord = exceptionInfo->ExceptionRecord;

		switch (exceptionRecord->ExceptionCode) {
		case DBG_PRINTEXCEPTION_WIDE_C:
		case DBG_PRINTEXCEPTION_C:
			if (exceptionRecord->NumberParameters >= 2) {
				ULONG len = (ULONG)exceptionRecord->ExceptionInformation[0];

				union {
					ULONG_PTR up;
					PCWSTR pwz;
					PCSTR psz;
				};

				up = exceptionRecord->ExceptionInformation[1];

				HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);

				if (exceptionRecord->ExceptionCode == DBG_PRINTEXCEPTION_C) {
					// Localized text will be incorrect displayed, if used not CP_OEMCP encoding.
					// WriteConsoleA(hOut, psz, len, &len, 0);

					// assume CP_ACP encoding
					if (ULONG n = MultiByteToWideChar(CP_ACP, 0, psz, len, nullptr, 0)) {
						PWSTR wz = (PWSTR)alloca(n * sizeof(WCHAR));

						if (len = MultiByteToWideChar(CP_ACP, 0, psz, len, wz, n)) {
							pwz = wz;
						}
					}
				}

				if (len) {
					WriteConsoleW(hOut, pwz, len - 1, &len, nullptr);
				}
			}

			return EXCEPTION_CONTINUE_EXECUTION;
		}

		return EXCEPTION_CONTINUE_SEARCH;
	}

	Button MapVKCodeToRawButton(uint32 vkCode) {
		if (vkCode >= '0' and vkCode <= '9') {
			return (Button)(vkCode + EButton_0 - '0');
		}
		if (vkCode >= 'A' and vkCode <= 'Z') {
			return (Button)(vkCode + EButton_a - 'A');
		}
		if (vkCode >= VK_F1 and vkCode <= VK_F12) {
			return (Button)(vkCode + EButton_f1 - VK_F1);
		}
		switch (vkCode) {
		case VK_SPACE:
			return EButton_space;
		case VK_TAB:
			return EButton_tab;
		case VK_RETURN:
			return EButton_enter;
		case VK_SHIFT:
			return EButton_shift;
		case VK_CONTROL:
			return EButton_ctrl;
		case VK_ESCAPE:
			return EButton_esc;
		case VK_UP:
			return EButton_up;
		case VK_DOWN:
			return EButton_down;
		case VK_LEFT:
			return EButton_left;
		case VK_RIGHT:
			return EButton_right;
		case VK_MENU:
			return EButton_alt;
		case VK_BACK:
			return EButton_backspace;
		case VK_DELETE:
			return EButton_delete;
		}
	}
}

bool Application::NewFrame() {
	bool result = HandleWindowsMessages();

	// Quit when escape is pressed, but not if in combination with ctrl or shift. This combination is usually pressed to open the task manager.
	if (ButtonDownEffect(_input, EButton_esc) and not(IsDown(_input, EButton_ctrl) || IsDown(_input, EButton_shift))) {
		result = false;
	}
	return result;
}

bool Application::HandleWindowsMessages() {
	bool running = true;

	for (int buttonIndex = 0; buttonIndex < EButton_count; ++buttonIndex) {
		_input.Keyboard[buttonIndex].WasDown = _input.Keyboard[buttonIndex].IsDown;
	}
	_input.Mouse.Left.WasDown = _input.Mouse.Left.IsDown;
	_input.Mouse.Right.WasDown = _input.Mouse.Right.IsDown;
	_input.Mouse.Middle.WasDown = _input.Mouse.Middle.IsDown;
	_input.Mouse.Scroll = 0.f;

	int oldMouseX = _input.Mouse.X;
	int oldMouseY = _input.Mouse.Y;
	float oldMouseRelX = _input.Mouse.RelX;
	float oldMouseRelY = _input.Mouse.RelY;

	MSG msg = {};

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			running = false;
		}

		if (_mainWindow.IsOpen() and _mainWindow.GetWindow() == msg.hwnd) {
			switch (msg.message) {
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP: {
				uint32 vkCode = (uint32)msg.wParam;
				bool wasDown = ((msg.lParam & (1 << 30)) != 0);
				bool isDown = ((msg.lParam & (1 << 31)) == 0);
				Button button = MapVKCodeToRawButton(vkCode);
				if (button != EButton_unknown) {
					_input.Keyboard[button].IsDown = isDown;
					_input.Keyboard[button].WasDown = wasDown;
				}
				if (button == EButton_alt) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				if (vkCode == VK_F4 and msg.lParam & 1 << 29) { // alt + f4
					running = false;
				}
				TranslateMessage(&msg); // This generates the WM_CHAR message.
				DispatchMessage(&msg);
				break;
			}

			 // The default window procedure will play a system notification sound 
			 // when pressing the Alt+Enter keyboard combination if this message is 
			 // not handled.
			case WM_SYSCHAR:
			case WM_CHAR:
				break;
			case WM_LBUTTONDOWN: {
				_input.Mouse.Left.IsDown = true;
				_input.Mouse.Left.WasDown = false;
				break;
			}
			case WM_LBUTTONUP: {
				_input.Mouse.Left.IsDown = false;
				_input.Mouse.Left.WasDown = true;
				break;
			}
			case WM_RBUTTONDOWN: {
				_input.Mouse.Right.IsDown = true;
				_input.Mouse.Right.WasDown = false;
				break;
			}
			case WM_RBUTTONUP: {
				_input.Mouse.Right.IsDown = false;
				_input.Mouse.Right.WasDown = true;
				break;
			}
			case WM_MBUTTONDOWN: {
				_input.Mouse.Middle.IsDown = true;
				_input.Mouse.Middle.WasDown = false;
				break;
			} 
			case WM_MBUTTONUP: {
				_input.Mouse.Middle.IsDown = false;
				_input.Mouse.Middle.WasDown = true;
				break;
			}
			case WM_MOUSEMOVE: {
				int mousePosX = GET_X_LPARAM(msg.lParam);
				int mousePosY = GET_Y_LPARAM(msg.lParam);
				_input.Mouse.X = mousePosX;
				_input.Mouse.Y = mousePosY;
				break;
			}
			case WM_MOUSEWHEEL: {
				float scroll = GET_WHEEL_DELTA_WPARAM(msg.wParam) / 120.f;
				_input.Mouse.Scroll = scroll;
				break;
			}
			default: {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				break;
			}
			}
		}
		else {
			if (msg.message == WM_KEYDOWN) {
				uint32 vkCode = (uint32)msg.wParam;
				if (vkCode == VK_F4 and (msg.lParam & (1 << 29)) or vkCode == VK_ESCAPE) {
					running = false;
				}
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	_input.Mouse.RelX = (float)_input.Mouse.X / (_mainWindow.ClientWidth - 1);
	_input.Mouse.RelY = (float)_input.Mouse.X / (_mainWindow.ClientHeight - 1);
	_input.Mouse.Dx = _input.Mouse.X - oldMouseX;
	_input.Mouse.Dy = _input.Mouse.X - oldMouseY;
	_input.Mouse.RelDx = _input.Mouse.RelX - oldMouseRelX;
	_input.Mouse.RelDy = _input.Mouse.RelY - oldMouseRelY;

	if (NumOpenWindows == 0) {
		running = false;
	}

	return running;
}

uint64 Application::RenderToWindow(float* clearColor) {
	DxResource frameResult = DxRenderer::Instance()->RenderTarget.ColorAttachments[0];
	DxResource backBuffer = _mainWindow.GetCurrentBackBuffer();
	
	DxCommandList* cl = DxContext::Instance().GetFreeRenderCommandList();

	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)_mainWindow.ClientWidth, (float)_mainWindow.ClientHeight);
	
	cl->SetScissor(scissorRect);
	cl->SetViewport(viewport);

	BarrierBatcher(cl).Transition(backBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RESOLVE_DEST)
				      .Transition(frameResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	cl->ResolveSubresource(backBuffer, 0, frameResult, 0, _mainWindow.GetBackBufferFormat());
	BarrierBatcher(cl).Transition(frameResult, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
				      .Transition(backBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT);
	
	uint64 result = DxContext::Instance().ExecuteCommandList(cl);

	_mainWindow.SwapBuffers();

	return result;
}

bool Application::Initialize() {
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	AddVectoredExceptionHandler(TRUE, HandleVectoredException);

	constexpr uint32 initialWidth = 1280;
	constexpr uint32 initialHeight = 800;

	DxRenderer* renderer = DxRenderer::Instance();
	renderer->Initialize(initialWidth, initialHeight);

	if (not _mainWindow.Initialize(TEXT("Main Window"), initialWidth, initialHeight)) {
		return false;
	}
	NumOpenWindows++;


	_timer.Reset();
	return true;
}

void Application::Run() {
	DxContext& dxContext = DxContext::Instance();
	float dt;

	uint64 fenceValues[NUM_BUFFERED_FRAMES] = {};
	uint64 frameId = 0;

	fenceValues[NUM_BUFFERED_FRAMES - 1] = dxContext.RenderQueue.Signal();

	DxRenderer* renderer = DxRenderer::Instance();
	
	while (NewFrame()) {
		dxContext.RenderQueue.WaitForFence(fenceValues[_mainWindow.CurrentBackBufferIndex()]);
		dxContext.NewFrame(frameId);

		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtv = _mainWindow.Rtv();
		const DxResource backBuffer = _mainWindow.GetCurrentBackBuffer();

		renderer->BeginFrame(rtv, backBuffer);
		fenceValues[_mainWindow.CurrentBackBufferIndex()] = renderer->DummyRender();

		float clearColor1[] = { 1.f, 0.f, 0.f, 1.f };

		// fenceValues[_mainWindow.CurrentBackBufferIndex] = RenderToWindow(clearColor1);

		_mainWindow.SwapBuffers();
		++frameId;
	}

	dxContext.Quit();
}
