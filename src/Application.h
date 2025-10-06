#pragma once

#include "pch.h"
#include "input.h"
#include "DirectWindow.h"
#include "GameTimer.h"
#include "directx/DxPipeline.h"

class Application {
public:
	bool HandleWindowsMessages();
	bool NewFrame();
	uint64 RenderToWindow(float* clearColor);
	void Run();
	bool Initialize();
	static Application* Instance() {
		return _instance;
	}
	uint32 NumOpenWindows = 0;
private:
	static Application* _instance;
	UserInput _input = {};
	DirectWindow _mainWindow = {};
	GameTimer _timer;
};
