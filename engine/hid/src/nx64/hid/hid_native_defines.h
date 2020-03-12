#ifndef HID_NATIVE_DEFINES_H
#define HID_NATIVE_DEFINES_H

#define HID_NATIVE_MAX_GAMEPAD_COUNT 			8
#define HID_NATIVE_MAX_GAMEPAD_AXIS_COUNT 		32
#define HID_NATIVE_MAX_GAMEPAD_BUTTON_COUNT 	32
#define HID_NATIVE_MAX_GAMEPAD_HAT_COUNT 		0
#define HID_NATIVE_MAX_TOUCH_COUNT 				16 // TouchScreenStateCountMax

/*
enum GamepadButton
{
	GAMEPAD_BUTTON_A,
	GAMEPAD_BUTTON_B,
	GAMEPAD_BUTTON_DOWN,
	GAMEPAD_BUTTON_L,
	GAMEPAD_BUTTON_LEFT,
	GAMEPAD_BUTTON_MINUS,
	GAMEPAD_BUTTON_PLUS,
	GAMEPAD_BUTTON_R,
	GAMEPAD_BUTTON_RIGHT,
	GAMEPAD_BUTTON_STICKL,
	GAMEPAD_BUTTON_STICKLDOWN,
	GAMEPAD_BUTTON_STICKLLEFT,
	GAMEPAD_BUTTON_STICKLRIGHT,
	GAMEPAD_BUTTON_STICKLUP,
	GAMEPAD_BUTTON_STICKR,
	GAMEPAD_BUTTON_STICKRDOWN,
	GAMEPAD_BUTTON_STICKRLEFT,
	GAMEPAD_BUTTON_STICKRRIGHT,
	GAMEPAD_BUTTON_STICKRUP,
	GAMEPAD_BUTTON_UP,
	GAMEPAD_BUTTON_X,
	GAMEPAD_BUTTON_Y,
	GAMEPAD_BUTTON_ZL,
	GAMEPAD_BUTTON_ZR,

	GAMEPAD_BUTTON_MAX
};
*/

#define HID_KEY_SPACE 		' '
#define HID_KEY_EXCLAIM 	'!'
#define HID_KEY_QUOTEDBL 	'"'
#define HID_KEY_HASH 		'#'
#define HID_KEY_DOLLAR 		'$'
#define HID_KEY_AMPERSAND 	'&'
#define HID_KEY_QUOTE 		'\''
#define HID_KEY_LPAREN 		'('
#define HID_KEY_RPAREN 		')'
#define HID_KEY_ASTERISK 	'*'
#define HID_KEY_PLUS 		'+'
#define HID_KEY_COMMA 		','
#define HID_KEY_MINUS 		'-'
#define HID_KEY_PERIOD 		'.'
#define HID_KEY_SLASH 		'/'

#define HID_KEY_0 			'0'
#define HID_KEY_1 			'1'
#define HID_KEY_2 			'2'
#define HID_KEY_3 			'3'
#define HID_KEY_4 			'4'
#define HID_KEY_5 			'5'
#define HID_KEY_6 			'6'
#define HID_KEY_7 			'7'
#define HID_KEY_8 			'8'
#define HID_KEY_9 			'9'

#define HID_KEY_COLON ':'
#define HID_KEY_SEMICOLON ';'
#define HID_KEY_LESS '<'
#define HID_KEY_EQUALS '='
#define HID_KEY_GREATER '>'
#define HID_KEY_QUESTION '?'
#define HID_KEY_AT '@'

#define HID_KEY_A 'A'
#define HID_KEY_B 'B'
#define HID_KEY_C 'C'
#define HID_KEY_D 'D'
#define HID_KEY_E 'E'
#define HID_KEY_F 'F'
#define HID_KEY_G 'G'
#define HID_KEY_H 'H'
#define HID_KEY_I 'I'
#define HID_KEY_J 'J'
#define HID_KEY_K 'K'
#define HID_KEY_L 'L'
#define HID_KEY_M 'M'
#define HID_KEY_N 'N'
#define HID_KEY_O 'O'
#define HID_KEY_P 'P'
#define HID_KEY_Q 'Q'
#define HID_KEY_R 'R'
#define HID_KEY_S 'S'
#define HID_KEY_T 'T'
#define HID_KEY_U 'U'
#define HID_KEY_V 'V'
#define HID_KEY_W 'W'
#define HID_KEY_X 'X'
#define HID_KEY_Y 'Y'
#define HID_KEY_Z 'Z'

#define HID_KEY_LBRACKET '['
#define HID_KEY_BACKSLASH '\\'
#define HID_KEY_RBRACKET ']'
#define HID_KEY_CARET '^'
#define HID_KEY_UNDERSCORE '_'
#define HID_KEY_BACKQUOTE '`'

#define HID_KEY_LBRACE '{'
#define HID_KEY_PIPE '|'
#define HID_KEY_RBRACE '}'
#define HID_KEY_TILDE '~'

#define HID_KEY_ESC 0
#define HID_KEY_F1 0
#define HID_KEY_F2 0
#define HID_KEY_F3 0
#define HID_KEY_F4 0
#define HID_KEY_F5 0
#define HID_KEY_F6 0
#define HID_KEY_F7 0
#define HID_KEY_F8 0
#define HID_KEY_F9 0
#define HID_KEY_F10 0
#define HID_KEY_F11 0
#define HID_KEY_F12 0
#define HID_KEY_UP 0
#define HID_KEY_DOWN 0
#define HID_KEY_LEFT 0
#define HID_KEY_RIGHT 0

#define HID_KEY_TAB 0
#define HID_KEY_ENTER 0
#define HID_KEY_BACKSPACE 0
#define HID_KEY_INSERT 0
#define HID_KEY_DEL 0
#define HID_KEY_PAGEUP 0
#define HID_KEY_PAGEDOWN 0
#define HID_KEY_HOME 0
#define HID_KEY_END 0

#define HID_KEY_KP_0 0
#define HID_KEY_KP_1 0
#define HID_KEY_KP_2 0
#define HID_KEY_KP_3 0
#define HID_KEY_KP_4 0
#define HID_KEY_KP_5 0
#define HID_KEY_KP_6 0
#define HID_KEY_KP_7 0
#define HID_KEY_KP_8 0
#define HID_KEY_KP_9 0
#define HID_KEY_KP_DIVIDE 0
#define HID_KEY_KP_MULTIPLY 0
#define HID_KEY_KP_SUBTRACT 0
#define HID_KEY_KP_ADD 0
#define HID_KEY_KP_DECIMAL 0
#define HID_KEY_KP_EQUAL 0
#define HID_KEY_KP_ENTER 0
#define HID_KEY_KP_NUM_LOCK 0
#define HID_KEY_BACK 0

// Modifiers
// file:///C:/Nintendo/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/structnn_1_1hid_1_1_keyboard_modifier.html
#define HID_KEY_LSHIFT 		0
#define HID_KEY_RSHIFT 		0
#define HID_KEY_LCTRL 		0
#define HID_KEY_RCTRL 		0
#define HID_KEY_LALT 		0
#define HID_KEY_RALT 		0
#define HID_KEY_PAUSE 		0
#define HID_KEY_CAPS_LOCK 	0
#define HID_KEY_SCROLL_LOCK 0
#define HID_KEY_LSUPER 		0
#define HID_KEY_RSUPER 		0
#define HID_KEY_MENU 		0

#define HID_KEY_MAX HID_KEY_BACK

// file:///C:/Nintendo/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/structnn_1_1hid_1_1_mouse_button.html

#define HID_MOUSE_BUTTON_LEFT 	0
#define HID_MOUSE_BUTTON_MIDDLE 0
#define HID_MOUSE_BUTTON_RIGHT 	0
#define HID_MOUSE_BUTTON_1 		0
#define HID_MOUSE_BUTTON_2 		0
#define HID_MOUSE_BUTTON_3 		0
#define HID_MOUSE_BUTTON_4 		0
#define HID_MOUSE_BUTTON_5 		0
#define HID_MOUSE_BUTTON_6 		0
#define HID_MOUSE_BUTTON_7 		0
#define HID_MOUSE_BUTTON_8 		0

#endif
