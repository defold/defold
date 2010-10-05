#include "hid.h"
#include "hid_private.h"

#include <string.h>

namespace dmHID
{
    extern const char* KEY_NAMES[MAX_KEY_COUNT];
    extern const char* MOUSE_BUTTON_NAMES[MAX_MOUSE_BUTTON_COUNT];

    HGamepad GetGamepad(uint8_t index)
    {
        if (index < MAX_GAMEPAD_COUNT)
            return &s_Context->m_Gamepads[index];
        else
            return INVALID_GAMEPAD_HANDLE;
    }

    uint32_t GetGamepadButtonCount(HGamepad gamepad)
    {
        return gamepad->m_ButtonCount;
    }

    uint32_t GetGamepadAxisCount(HGamepad gamepad)
    {
        return gamepad->m_AxisCount;
    }

    bool IsKeyboardConnected()
    {
        return s_Context->m_KeyboardConnected;
    }

    bool IsMouseConnected()
    {
        return s_Context->m_MouseConnected;
    }

    bool IsGamepadConnected(HGamepad gamepad)
    {
        if (gamepad != 0x0)
            return gamepad->m_Connected;
        else
            return false;
    }

    bool GetKeyboardPacket(KeyboardPacket* packet)
    {
        if (packet != 0x0)
        {
            *packet = s_Context->m_KeyboardPacket;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetMousePacket(MousePacket* packet)
    {
        if (packet != 0x0)
        {
            *packet = s_Context->m_MousePacket;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetGamepadPacket(HGamepad gamepad, GamepadPacket* packet)
    {
        if (gamepad != 0x0 && packet != 0x0)
        {
            *packet = gamepad->m_Packet;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetKey(KeyboardPacket* packet, Key key)
    {
        if (packet != 0x0)
            return packet->m_Keys[key / 32] & (1 << (key % 32));
        else
            return false;
    }

    void SetKey(Key key, bool value)
    {
        if (s_Context != 0x0)
        {
            if (value)
                s_Context->m_KeyboardPacket.m_Keys[key / 32] |= (1 << (key % 32));
            else
                s_Context->m_KeyboardPacket.m_Keys[key / 32] &= ~(1 << (key % 32));
        }
    }

    bool GetMouseButton(MousePacket* packet, MouseButton button)
    {
        if (packet != 0x0)
            return packet->m_Buttons[button / 32] & (1 << (button % 32));
        else
            return false;
    }

    void SetMouseButton(MouseButton button, bool value)
    {
        if (s_Context != 0x0)
        {
            if (value)
                s_Context->m_MousePacket.m_Buttons[button / 32] |= (1 << (button % 32));
            else
                s_Context->m_MousePacket.m_Buttons[button / 32] &= ~(1 << (button % 32));
        }
    }

    void SetMousePosition(int32_t x, int32_t y)
    {
        if (s_Context != 0x0)
        {
            s_Context->m_MousePacket.m_PositionX = x;
            s_Context->m_MousePacket.m_PositionY = y;
        }
    }

    void SetMouseWheel(int32_t value)
    {
        if (s_Context != 0x0)
        {
            s_Context->m_MousePacket.m_Wheel = value;
        }
    }

    bool GetGamepadButton(GamepadPacket* packet, uint32_t button)
    {
        if (packet != 0x0)
            return packet->m_Buttons[button / 32] & (1 << (button % 32));
        else
            return false;
    }

    void SetGamepadButton(HGamepad gamepad, uint32_t button, bool value)
    {
        if (gamepad != 0x0)
        {
            if (value)
                gamepad->m_Packet.m_Buttons[button / 32] |= (1 << (button % 32));
            else
                gamepad->m_Packet.m_Buttons[button / 32] &= ~(1 << (button % 32));
        }
    }

    void SetGamepadAxis(HGamepad gamepad, uint32_t axis, float value)
    {
        if (gamepad != 0x0)
        {
            gamepad->m_Packet.m_Axis[axis] = value;
        }
    }

    const char* GetKeyName(Key key)
    {
        return KEY_NAMES[key];
    }

    const char* GetMouseButtonName(MouseButton input)
    {
        return MOUSE_BUTTON_NAMES[input];
    }

    const char* KEY_NAMES[MAX_KEY_COUNT] =
    {
        "KEY_SPACE",
        "KEY_EXCLAIM",
        "KEY_QUOTEDBL",
        "KEY_HASH",
        "KEY_DOLLAR",
        "KEY_AMPERSAND",
        "KEY_QUOTE",
        "KEY_LPAREN",
        "KEY_RPAREN",
        "KEY_ASTERISK",
        "KEY_PLUS",
        "KEY_COMMA",
        "KEY_MINUS",
        "KEY_PERIOD",
        "KEY_SLASH",
        "KEY_0",
        "KEY_1",
        "KEY_2",
        "KEY_3",
        "KEY_4",
        "KEY_5",
        "KEY_6",
        "KEY_7",
        "KEY_8",
        "KEY_9",
        "KEY_COLON",
        "KEY_SEMICOLON",
        "KEY_LESS",
        "KEY_EQUALS",
        "KEY_GREATER",
        "KEY_QUESTION",
        "KEY_AT",
        "KEY_A",
        "KEY_B",
        "KEY_C",
        "KEY_D",
        "KEY_E",
        "KEY_F",
        "KEY_G",
        "KEY_H",
        "KEY_I",
        "KEY_J",
        "KEY_K",
        "KEY_L",
        "KEY_M",
        "KEY_N",
        "KEY_O",
        "KEY_P",
        "KEY_Q",
        "KEY_R",
        "KEY_S",
        "KEY_T",
        "KEY_U",
        "KEY_V",
        "KEY_W",
        "KEY_X",
        "KEY_Y",
        "KEY_Z",
        "KEY_LBRACKET",
        "KEY_BACKSLASH",
        "KEY_RBRACKET",
        "KEY_CARET",
        "KEY_UNDERSCORE",
        "KEY_BACKQUOTE",
        "KEY_LBRACE",
        "KEY_PIPE",
        "KEY_RBRACE",
        "KEY_TILDE",
        "KEY_ESC",
        "KEY_F1",
        "KEY_F2",
        "KEY_F3",
        "KEY_F4",
        "KEY_F5",
        "KEY_F6",
        "KEY_F7",
        "KEY_F8",
        "KEY_F9",
        "KEY_F10",
        "KEY_F11",
        "KEY_F12",
        "KEY_UP",
        "KEY_DOWN",
        "KEY_LEFT",
        "KEY_RIGHT",
        "KEY_LSHIFT",
        "KEY_RSHIFT",
        "KEY_LCTRL",
        "KEY_RCTRL",
        "KEY_LALT",
        "KEY_RALT",
        "KEY_TAB",
        "KEY_ENTER",
        "KEY_BACKSPACE",
        "KEY_INSERT",
        "KEY_DEL",
        "KEY_PAGEUP",
        "KEY_PAGEDOWN",
        "KEY_HOME",
        "KEY_END",
        "KEY_KP_0",
        "KEY_KP_1",
        "KEY_KP_2",
        "KEY_KP_3",
        "KEY_KP_4",
        "KEY_KP_5",
        "KEY_KP_6",
        "KEY_KP_7",
        "KEY_KP_8",
        "KEY_KP_9",
        "KEY_KP_DIVIDE",
        "KEY_KP_MULTIPLY",
        "KEY_KP_SUBTRACT",
        "KEY_KP_ADD",
        "KEY_KP_DECIMAL",
        "KEY_KP_EQUAL",
        "KEY_KP_ENTER"
    };

    const char* MOUSE_BUTTON_NAMES[MAX_MOUSE_BUTTON_COUNT] =
    {
        "MOUSE_BUTTON_LEFT",
        "MOUSE_BUTTON_MIDDLE",
        "MOUSE_BUTTON_RIGHT"
    };
}
