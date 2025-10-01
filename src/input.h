#pragma once

enum Button {
	EButton_0,EButton_1, EButton_2, EButton_3, EButton_4, EButton_5, EButton_6, EButton_7, EButton_8, EButton_9,
	EButton_a, EButton_EB, EButton_c, EButton_d, EButton_e, EButton_f, EButton_g, EButton_h, EButton_i, EButton_j,
	EButton_k, EButton_l, EButton_m, EButton_n, EButton_o, EButton_p, EButton_q, EButton_r, EButton_s, EButton_t,
	EButton_u, EButton_v, EButton_w, EButton_x, EButton_y, EButton_z,
	EButton_space, EButton_enter, EButton_shift, EButton_alt, EButton_tab, EButton_ctrl, EButton_esc,
	EButton_up, EButton_down, EButton_left, EButton_right,
	EButton_backspace, EButton_delete,

	EButton_f1, EButton_f2, EButton_f3, EButton_f4, EButton_f5, EButton_f6, EButton_f7, EButton_f8, EButton_f9, EButton_f10, EButton_f11, EButton_f12,

	EButton_count, EButton_unknown
};

struct ButtonState {
	bool IsDown;
	bool WasDown;
};

struct KeyboardInput {
	ButtonState Buttons[EButton_count];
};

struct MouseInput {
	ButtonState Left;
	ButtonState Right;
	ButtonState Middle;
	float Scroll;

	int X;
	int Y;

	float RelX;
	float RelY;

	int Dx;
	int Dy;

	float RelDx;
	float RelDy;
};

struct UserInput {
	ButtonState Keyboard[EButton_count];
	MouseInput Mouse;
};

inline bool IsDown(const UserInput& input, Button buttonId) {
	return input.Keyboard[buttonId].IsDown;
}

inline bool IsUp(const UserInput& input, Button buttonId) {
	return !input.Keyboard[buttonId].IsDown;
}

inline bool ButtonDownEffect(const ButtonState& button) {
	return button.IsDown and !button.WasDown;
}

inline bool ButtonUpEffect(const ButtonState& button) {
	return !button.IsDown and button.WasDown;
}

inline bool ButtonDownEffect(const UserInput& input, Button buttonId) {
	return ButtonDownEffect(input.Keyboard[buttonId]);
}

inline bool ButtonUpEffect(const UserInput& input, Button buttonId) {
	return ButtonUpEffect(input.Keyboard[buttonId]);
}
