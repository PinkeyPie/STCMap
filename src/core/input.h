#pragma once
#include "../pch.h"
#include <winuser.h>

enum KeyCode {
	EKey_Shift = VK_SHIFT,
	EKey_Ctrl = VK_CONTROL,
	EKey_Alt = VK_MENU,
	EKey_Esc = VK_ESCAPE,
	EKey_Caps = VK_CAPITAL,
	EKey_Space = VK_SPACE,
	EKey_Enter = VK_RETURN,
	EKey_Backspace = VK_BACK,
	EKey_Tab = VK_TAB,
	EKey_Left = VK_LEFT,
	EKey_Right = VK_RIGHT,
	EKey_Up = VK_UP,
	EKey_Down = VK_DOWN,
	EKey_F1 = VK_F1,
	EKey_F2 = VK_F2,
	EKey_F3 = VK_F3,
	EKey_F4 = VK_F4,
	EKey_F5 = VK_F5,
	EKey_F6 = VK_F6,
	EKey_F7 = VK_F7,
	EKey_F8 = VK_F8,
	EKey_F9 = VK_F9,
	EKey_F10 = VK_F10,
	EKey_F11 = VK_F11,
	EKey_F12 = VK_F12,
};

struct InputKey {
	bool Down;
	bool PressEvent;
};

struct InputMouseButton {
	bool Down;
	bool ClickEvent;
	bool DoubleClicked;
};

struct MouseInput {
	InputMouseButton Left;
	InputMouseButton Right;
	InputMouseButton Middle;
	float Scroll;

	int32 X;
	int32 Y;
	int32 Dx;
	int32 Dy;

	float RelX;
	float RelY;

	float RelDx;
	float RelDy;
};

struct UserInput {
	InputKey Keyboard[128];
	MouseInput Mouse;
	bool OverWindow;
};
