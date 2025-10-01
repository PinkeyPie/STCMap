#pragma once

#include "pch.h"
#include "input.h"
#include "DirectWindow.h"
#include "GameTimer.h"

class Application {
public:
	bool HandleWindowsMessages();
	bool NewFrame();
	uint64 RenderToWindow(float* clearColor);
	void Run();
	bool Initialize();
private:
	UserInput _input = {};
	DirectWindow _mainWindow = {};
	GameTimer _timer;
};
