#ifndef DM_HID_H
#define DM_HID_H

// Winuser.h defines MAX_TOUCH_COUNT to 256, which clashes with dmHID::MAX_TOUCH_COUNT
// https://msdn.microsoft.com/en-us/library/windows/desktop/hh802879(v=vs.85).aspx
#undef MAX_TOUCH_COUNT

#include <stdint.h>

// Let glfw decide the constants
#include "hid_glfw_defines.h"

namespace dmHID
{
    const uint32_t MAX_CHAR_COUNT = 256;

    /// Hid context handle
    typedef struct Context* HContext;
    /// Gamepad handle
    typedef struct Gamepad* HGamepad;

    /// Constant that defines invalid context handles
    const HContext INVALID_CONTEXT = 0;
    /// Constant that defines invalid gamepad handles
    const HGamepad INVALID_GAMEPAD_HANDLE = 0;

    /// Maximum number of gamepads supported
    const static uint32_t MAX_GAMEPAD_COUNT = HID_MAX_GAMEPAD_COUNT;
    /// Maximum number of gamepad axis supported
    const static uint32_t MAX_GAMEPAD_AXIS_COUNT = 32;
    /// Maximum number of gamepad buttons supported
    const static uint32_t MAX_GAMEPAD_BUTTON_COUNT = 32;

    /// Maximum number of simultaneous touches supported
    // An iPad supports a maximum of 11 simultaneous touches
    const static uint32_t MAX_TOUCH_COUNT = 11;

    enum Key
    {
        KEY_SPACE = HID_KEY_SPACE,
        KEY_EXCLAIM = HID_KEY_EXCLAIM,
        KEY_QUOTEDBL = HID_KEY_QUOTEDBL,
        KEY_HASH = HID_KEY_HASH,
        KEY_DOLLAR = HID_KEY_DOLLAR,
        KEY_AMPERSAND = HID_KEY_AMPERSAND,
        KEY_QUOTE = HID_KEY_QUOTE,
        KEY_LPAREN = HID_KEY_LPAREN,
        KEY_RPAREN = HID_KEY_RPAREN,
        KEY_ASTERISK = HID_KEY_ASTERISK,
        KEY_PLUS = HID_KEY_PLUS,
        KEY_COMMA = HID_KEY_COMMA,
        KEY_MINUS = HID_KEY_MINUS,
        KEY_PERIOD = HID_KEY_PERIOD,
        KEY_SLASH = HID_KEY_SLASH,

        KEY_0 = HID_KEY_0,
        KEY_1 = HID_KEY_1,
        KEY_2 = HID_KEY_2,
        KEY_3 = HID_KEY_3,
        KEY_4 = HID_KEY_4,
        KEY_5 = HID_KEY_5,
        KEY_6 = HID_KEY_6,
        KEY_7 = HID_KEY_7,
        KEY_8 = HID_KEY_8,
        KEY_9 = HID_KEY_9,

        KEY_COLON = HID_KEY_COLON,
        KEY_SEMICOLON = HID_KEY_SEMICOLON,
        KEY_LESS = HID_KEY_LESS,
        KEY_EQUALS = HID_KEY_EQUALS,
        KEY_GREATER = HID_KEY_GREATER,
        KEY_QUESTION = HID_KEY_QUESTION,
        KEY_AT = HID_KEY_AT,

        KEY_A = HID_KEY_A,
        KEY_B = HID_KEY_B,
        KEY_C = HID_KEY_C,
        KEY_D = HID_KEY_D,
        KEY_E = HID_KEY_E,
        KEY_F = HID_KEY_F,
        KEY_G = HID_KEY_G,
        KEY_H = HID_KEY_H,
        KEY_I = HID_KEY_I,
        KEY_J = HID_KEY_J,
        KEY_K = HID_KEY_K,
        KEY_L = HID_KEY_L,
        KEY_M = HID_KEY_M,
        KEY_N = HID_KEY_N,
        KEY_O = HID_KEY_O,
        KEY_P = HID_KEY_P,
        KEY_Q = HID_KEY_Q,
        KEY_R = HID_KEY_R,
        KEY_S = HID_KEY_S,
        KEY_T = HID_KEY_T,
        KEY_U = HID_KEY_U,
        KEY_V = HID_KEY_V,
        KEY_W = HID_KEY_W,
        KEY_X = HID_KEY_X,
        KEY_Y = HID_KEY_Y,
        KEY_Z = HID_KEY_Z,

        KEY_LBRACKET = HID_KEY_LBRACKET,
        KEY_BACKSLASH = HID_KEY_BACKSLASH,
        KEY_RBRACKET = HID_KEY_RBRACKET,
        KEY_CARET = HID_KEY_CARET,
        KEY_UNDERSCORE = HID_KEY_UNDERSCORE,
        KEY_BACKQUOTE = HID_KEY_BACKQUOTE,

        KEY_LBRACE = HID_KEY_LBRACE,
        KEY_PIPE = HID_KEY_PIPE,
        KEY_RBRACE = HID_KEY_RBRACE,
        KEY_TILDE = HID_KEY_TILDE,
        /* End of ASCII mapped keysyms */

        /* Special keys */
        KEY_ESC = HID_KEY_ESC,
        KEY_F1 = HID_KEY_F1,
        KEY_F2 = HID_KEY_F2,
        KEY_F3 = HID_KEY_F3,
        KEY_F4 = HID_KEY_F4,
        KEY_F5 = HID_KEY_F5,
        KEY_F6 = HID_KEY_F6,
        KEY_F7 = HID_KEY_F7,
        KEY_F8 = HID_KEY_F8,
        KEY_F9 = HID_KEY_F9,
        KEY_F10 = HID_KEY_F10,
        KEY_F11 = HID_KEY_F11,
        KEY_F12 = HID_KEY_F12,
        KEY_UP = HID_KEY_UP,
        KEY_DOWN = HID_KEY_DOWN,
        KEY_LEFT = HID_KEY_LEFT,
        KEY_RIGHT = HID_KEY_RIGHT,
        KEY_LSHIFT = HID_KEY_LSHIFT,
        KEY_RSHIFT = HID_KEY_RSHIFT,
        KEY_LCTRL = HID_KEY_LCTRL,
        KEY_RCTRL = HID_KEY_RCTRL,
        KEY_LALT = HID_KEY_LALT,
        KEY_RALT = HID_KEY_RALT,
        KEY_TAB = HID_KEY_TAB,
        KEY_ENTER = HID_KEY_ENTER,
        KEY_BACKSPACE = HID_KEY_BACKSPACE,
        KEY_INSERT = HID_KEY_INSERT,
        KEY_DEL = HID_KEY_DEL,
        KEY_PAGEUP = HID_KEY_PAGEUP,
        KEY_PAGEDOWN = HID_KEY_PAGEDOWN,
        KEY_HOME = HID_KEY_HOME,
        KEY_END = HID_KEY_END,
        KEY_KP_0 = HID_KEY_KP_0,
        KEY_KP_1 = HID_KEY_KP_1,
        KEY_KP_2 = HID_KEY_KP_2,
        KEY_KP_3 = HID_KEY_KP_3,
        KEY_KP_4 = HID_KEY_KP_4,
        KEY_KP_5 = HID_KEY_KP_5,
        KEY_KP_6 = HID_KEY_KP_6,
        KEY_KP_7 = HID_KEY_KP_7,
        KEY_KP_8 = HID_KEY_KP_8,
        KEY_KP_9 = HID_KEY_KP_9,
        KEY_KP_DIVIDE = HID_KEY_KP_DIVIDE,
        KEY_KP_MULTIPLY = HID_KEY_KP_MULTIPLY,
        KEY_KP_SUBTRACT = HID_KEY_KP_SUBTRACT,
        KEY_KP_ADD = HID_KEY_KP_ADD,
        KEY_KP_DECIMAL = HID_KEY_KP_DECIMAL,
        KEY_KP_EQUAL = HID_KEY_KP_EQUAL,
        KEY_KP_ENTER = HID_KEY_KP_ENTER,
        KEY_KP_NUM_LOCK = HID_KEY_KP_NUM_LOCK,
        KEY_CAPS_LOCK = HID_KEY_CAPS_LOCK,
        KEY_SCROLL_LOCK = HID_KEY_SCROLL_LOCK,
        KEY_PAUSE = HID_KEY_PAUSE,
        KEY_LSUPER = HID_KEY_LSUPER,
        KEY_RSUPER = HID_KEY_RSUPER,
        KEY_MENU = HID_KEY_MENU,
        KEY_BACK = HID_KEY_BACK,

        MAX_KEY_COUNT = HID_KEY_BACK + 1
    };

    enum KeyboardType
    {
        KEYBOARD_TYPE_DEFAULT,
        KEYBOARD_TYPE_NUMBER_PAD,
        KEYBOARD_TYPE_EMAIL,
        KEYBOARD_TYPE_PASSWORD,
    };

    enum MouseButton
    {
        MOUSE_BUTTON_LEFT = HID_MOUSE_BUTTON_LEFT,
        MOUSE_BUTTON_MIDDLE = HID_MOUSE_BUTTON_MIDDLE,
        MOUSE_BUTTON_RIGHT = HID_MOUSE_BUTTON_RIGHT,
        MOUSE_BUTTON_1 = HID_MOUSE_BUTTON_1,
        MOUSE_BUTTON_2 = HID_MOUSE_BUTTON_2,
        MOUSE_BUTTON_3 = HID_MOUSE_BUTTON_3,
        MOUSE_BUTTON_4 = HID_MOUSE_BUTTON_4,
        MOUSE_BUTTON_5 = HID_MOUSE_BUTTON_5,
        MOUSE_BUTTON_6 = HID_MOUSE_BUTTON_6,
        MOUSE_BUTTON_7 = HID_MOUSE_BUTTON_7,
        MOUSE_BUTTON_8 = HID_MOUSE_BUTTON_8,

        MAX_MOUSE_BUTTON_COUNT = HID_MOUSE_BUTTON_8 + 1
    };

    enum Phase
    {
        // NOTE: By convention the enumeration corresponds to iOS values
        PHASE_BEGAN,
        PHASE_MOVED,
        PHASE_STATIONARY,
        PHASE_ENDED,
        PHASE_CANCELLED,
    };

    struct KeyboardPacket
    {
        uint32_t m_Keys[MAX_KEY_COUNT / 32 + 1];
    };

    struct TextPacket
    {
        char     m_Text[MAX_CHAR_COUNT];
        uint32_t m_Size;
    };

    struct MarkedTextPacket
    {
        char     m_Text[MAX_CHAR_COUNT];
        uint32_t m_Size;
        uint32_t m_HasText : 1;
    };

    struct MousePacket
    {
        int32_t m_PositionX, m_PositionY;
        int32_t m_Wheel;
        uint32_t m_Buttons[MAX_MOUSE_BUTTON_COUNT / 32 + 1];
    };

    struct GamepadPacket
    {
        float m_Axis[MAX_GAMEPAD_AXIS_COUNT];
        uint32_t m_Buttons[MAX_GAMEPAD_BUTTON_COUNT / 32 + 1];
    };

    /**
     * Data for a single touch, e.g. finger
     */
    struct Touch
    {
        /// Single-click, double, etc
        int32_t m_TapCount;
        /// Begin, end, etc
        Phase   m_Phase;
        /// Current x
        int32_t m_X;
        /// Current y
        int32_t m_Y;
        /// Current x, in screen space
        int32_t m_ScreenX;
        /// Current y, in screen space
        int32_t m_ScreenY;
        /// Current dx
        int32_t m_DX;
        /// Current dy
        int32_t m_DY;
        /// Current dx, in screen space
        int32_t m_ScreenDX;
        /// Current dy, in screen space
        int32_t m_ScreenDY;
        /// Touch id
        int32_t m_Id;
    };

    /**
     * Data for the current touch-state
     */
    struct TouchDevicePacket
    {
        Touch    m_Touches[MAX_TOUCH_COUNT];
        uint32_t m_TouchCount;
    };

    struct AccelerationPacket
    {
        float m_X, m_Y, m_Z;
    };

    /// parameters to be passed to NewContext
    struct NewContextParams
    {
        NewContextParams();

        /// if mouse input should be ignored
        uint32_t m_IgnoreMouse : 1;
        /// if keyboard input should be ignored
        uint32_t m_IgnoreKeyboard : 1;
        /// if gamepad input should be ignored
        uint32_t m_IgnoreGamepads : 1;
        /// if touch device input should be ignored
        uint32_t m_IgnoreTouchDevice : 1;
        /// if acceleration input should be ignored
        uint32_t m_IgnoreAcceleration : 1;
        /// if mouse wheel scroll direction should be flipped (see DEF-2450)
        uint32_t m_FlipScrollDirection : 1;

    };

    /**
     * Creates a new hid context.
     *
     * @params parameters to use for the new context
     * @return a new hid context, or INVALID_CONTEXT if it could not be created
     */
    HContext NewContext(const NewContextParams& params);

    /**
     * Deletes a hid context.
     *
     * @param context context to be deleted
     */
    void DeleteContext(HContext context);

    /**
     * Initializes a hid context.
     *
     * @param context context to initialize
     * @return if the context was successfully initialized or not
     */
    bool Init(HContext context);

    /**
     * Finalizes a hid context.
     *
     * @param context context to finalize
     */
    void Final(HContext context);

    /**
     * Updates a hid context by polling input from the connected hid devices.
     *
     * @param context the context to poll from
     */
    void Update(HContext context);

    /**
     * Creates/opens a new gamepad.
     *
     * @param context context in which to find the gamepad
     * @param index Gamepad index
     * @return Handle to gamepad. NULL if not available
     */
    HGamepad GetGamepad(HContext context, uint8_t index);

    /**
     * Retrieves the number of buttons on a given gamepad.
     *
     * @param gamepad gamepad handle
     * @return the number of buttons
     */
    uint32_t GetGamepadButtonCount(HGamepad gamepad);

    /**
     * Retrieves the number of axis on a given gamepad.
     *
     * @param gamepad gamepad handle
     * @return the number of axis
     */
    uint32_t GetGamepadAxisCount(HGamepad gamepad);

    /**
     * Retrieves the platform-specific device name of a given gamepad.
     *
     * @param gamepad gamepad handle
     * @param a pointer to the device name, or 0x0 if not specified
     */
    void GetGamepadDeviceName(HGamepad gamepad, const char** out_device_name);

    /**
     * Check if a keyboard is connected.
     *
     * @param context context in which to search
     * @return If a keyboard is connected or not
     */
    bool IsKeyboardConnected(HContext context);

    /**
     * Check if a mouse is connected.
     *
     * @param context context in which to search
     * @return If a mouse is connected or not
     */
    bool IsMouseConnected(HContext context);

    /**
     * Check if the supplied gamepad is connected or not.
     *
     * @param gamepad Handle to gamepad
     * @return If the gamepad is connected or not
     */
    bool IsGamepadConnected(HGamepad gamepad);

    /**
     * Check if a touch device is connected.
     *
     * @param context context in which to search
     * @return If a touch device is connected or not
     */
    bool IsTouchDeviceConnected(HContext context);

    /**
     * Check if an accelerometer is connected.
     *
     * @param context context in which to search
     * @return If an accelerometer is connected or not
     */
    bool IsAccelerometerConnected(HContext context);

    /**
     * Obtain a keyboard packet reflecting the current input state of a HID context.
     * @note If no keyboard is connected, the internal buffers will not be changed, out_packet will not be updated,
     * and the function returns false.
     *
     * @param context context from which to retrieve the packet
     * @param out_packet Keyboard packet out argument
     * @return If the packet was successfully updated or not.
     */
    bool GetKeyboardPacket(HContext context, KeyboardPacket* out_packet);

    /**
     * Get text-input package
     * @note The function clears the internal buffer and subsequent calls will return an empty package (size 0)
     * If no keyboard is connected, the internal buffers will not be changed, out_packet will not be updated,
     * and the function returns false.
     * @param context context
     * @param out_packet package
     * @return If the packet was successfully updated or not.
     */
    bool GetTextPacket(HContext context, TextPacket* out_packet);

    /**
     * Get marked text-input package
     * @note The function clears the internal buffer and subsequent calls will return an empty package (size 0)
     * If no keyboard is connected, the internal buffers will not be changed, out_packet will not be updated,
     * and the function returns false.
     * @param context context
     * @param out_packet package
     * @return If the packet was successfully updated or not.
     */
    bool GetMarkedTextPacket(HContext context, MarkedTextPacket* out_packet);

    /**
     * Obtain a mouse packet reflecting the current input state of a HID context.
     *
     * @param context context from which to retrieve the packet
     * @param out_packet Mouse packet out argument
     * @return If the packet was successfully updated or not.
     */
    bool GetMousePacket(HContext context, MousePacket* out_packet);

    /**
     * Obtain a gamepad packet reflecting the current input state of the gamepad in a  HID context.
     *
     * @param gamepad gamepad handle
     * @param out_packet Gamepad packet out argument
     * @return If the packet was successfully updated or not.
     */
    bool GetGamepadPacket(HGamepad gamepad, GamepadPacket* out_packet);

    /**
     * Obtain a touch device packet reflecting the current input state of a HID context.
     *
     * @param context context from which to retrieve the packet
     * @param out_packet Touch device packet out argument
     * @return If the packet was successfully updated or not.
     */
    bool GetTouchDevicePacket(HContext context, TouchDevicePacket* out_packet);

    bool GetAccelerationPacket(HContext context, AccelerationPacket* out_packet);

    /**
     * Convenience function to retrieve the state of a key from a keyboard packet.
     *
     * @param packet Keyboard packet
     * @param key The requested key
     * @return If the key was pressed or not
     */
    bool GetKey(KeyboardPacket* packet, Key key);

    /**
     * Sets the state of a key.
     *
     * @param context context handle
     * @param key The requested key
     * @param value Key state
     */
    void SetKey(HContext context, Key key, bool value);

    /**
     * Add a keyboard character input
     *
     * @param context context handle
     * @param chr The character (unicode)
     */
    void AddKeyboardChar(HContext context, int chr);

    /**
     * Set current marked text
     *
     * @param context context handle
     * @param text The marked text string
     */
    void SetMarkedText(HContext context, char* text);

    /**
     * Show keyboard if applicable
     * @param context context
     * @param type keyboard type
     * @param autoclose close keyboard automatically when outside
     */
    void ShowKeyboard(HContext context, KeyboardType type, bool autoclose);

    /**
     * Hide keyboard
     * @param context context
     */
    void HideKeyboard(HContext context);

    /**
     * Reset keyboard
     * @param context context
     */
    void ResetKeyboard(HContext context);

    /**
     * Convenience function to retrieve the state of a mouse button from a mouse packet.
     *
     * @param packet Mouse packet
     * @param button The requested button
     * @return If the button was pressed or not
     */
    bool GetMouseButton(MousePacket* packet, MouseButton button);

    /**
     * Sets the state of a mouse button.
     *
     * @param context context handle
     * @param button The requested button
     * @param value Button state
     */
    void SetMouseButton(HContext context, MouseButton button, bool value);

    /**
     * Sets the position of a mouse.
     *
     * @param context context handle
     * @param x x-coordinate of the position
     * @param y y-coordinate of the position
     */
    void SetMousePosition(HContext context, int32_t x, int32_t y);

    /**
     * Sets the mouse wheel.
     *
     * @param context context handle
     * @param value wheel value
     */
    void SetMouseWheel(HContext context, int32_t value);

    /**
     * Convenience function to retrieve the state of a gamepad button from a gamepad packet.
     * @param packet Gamepad packet
     * @param button The requested button
     * @return If the button was pressed or not
     */
    bool GetGamepadButton(GamepadPacket* packet, uint32_t button);

    /**
     * Sets the state of a gamepad button.
     *
     * @param packet Gamepad packet
     * @param button The requested button
     * @param value Button state
     */
    void SetGamepadButton(HGamepad gamepad, uint32_t button, bool value);

    /**
     * Sets the state of a gamepad axis.
     *
     * @param packet Gamepad packet
     * @param axis The requested axis
     * @param value Button state
     */
    void SetGamepadAxis(HGamepad gamepad, uint32_t axis, float value);

    /**
     * Convenience function to retrieve the position of a specific touch.
     * Used in unit tests (test_input.cpp)
     *
     * @param touch_index which touch to get the position from
     * @param x x-coordinate as out-parameter
     * @param y y-coordinate as out-parameter
     * @param id identifier of touch as out-parameter
     * @param pressed boolean indicating if touch is pressed as out-parameter
     * @param released boolean indicating if touch is released as out-parameter
     * @return if the position could be retrieved
     */
    bool GetTouch(TouchDevicePacket* packet, uint32_t touch_index, int32_t* x, int32_t* y, uint32_t* id, bool* pressed = 0x0, bool* released = 0x0);

    /**
     * Adds of a touch.
     * Used in unit tests (test_input.cpp)
     *
     * @param context context handle
     * @param x x-coordinate of the position
     * @param y y-coordinate of the position
     * @param phase phase of touch
     * @param id identifier of touch
     */
    void AddTouch(HContext context, int32_t x, int32_t y, uint32_t id, Phase phase);

    /**
     * Clears all touches.
     * Used in unit tests (test_input.cpp)
     *
     * @param context context handle
     */
    void ClearTouches(HContext context);

    /**
     * Get the name of a keyboard key.
     * @param key Keyboard key
     * @return The name of the key
     */
    const char* GetKeyName(Key key);

    /**
     * Get the name of a mouse button.
     * @param button Mouse button
     * @return The name of the button
     */
    const char* GetMouseButtonName(MouseButton button);

    /**
     * Enables the accelerometer (if available)
     */
    void EnableAccelerometer();
}

#endif // DM_HID_H
