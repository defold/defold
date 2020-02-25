#include <assert.h>
#include <dlib/log.h>
#include "hid.h"
#include "hid_private.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/utf8.h>

namespace dmHID
{
    NewContextParams::NewContextParams()
    {
        memset(this, 0, sizeof(NewContextParams));
    }

    Context::Context()
    {
        memset(this, 0, sizeof(Context));
    }

    HContext NewContext(const NewContextParams& params)
    {
        HContext context = new Context();
        context->m_IgnoreMouse = params.m_IgnoreMouse;
        context->m_IgnoreKeyboard = params.m_IgnoreKeyboard;
        context->m_IgnoreGamepads = params.m_IgnoreGamepads;
        context->m_IgnoreTouchDevice = params.m_IgnoreTouchDevice;
        context->m_IgnoreAcceleration = params.m_IgnoreAcceleration;
        context->m_FlipScrollDirection = params.m_FlipScrollDirection;

        context->m_GamepadConnectivityCallback = params.m_GamepadConnectivityCallback;
        return context;
    }

    void SetGamepadFuncUserdata(HContext context, void* userdata)
    {
        context->m_GamepadConnectivityUserdata = userdata;
    }

    void DeleteContext(HContext context)
    {
        delete context;
    }

    HGamepad GetGamepad(HContext context, uint8_t index)
    {
        if (index < MAX_GAMEPAD_COUNT)
            return &context->m_Gamepads[index];
        else
            return INVALID_GAMEPAD_HANDLE;
    }

    uint32_t GetGamepadButtonCount(HGamepad gamepad)
    {
        return gamepad->m_ButtonCount;
    }

    uint32_t GetGamepadHatCount(HGamepad gamepad)
    {
        return gamepad->m_HatCount;
    }

    uint32_t GetGamepadAxisCount(HGamepad gamepad)
    {
        return gamepad->m_AxisCount;
    }

    bool IsKeyboardConnected(HContext context)
    {
        return context->m_KeyboardConnected;
    }

    bool IsMouseConnected(HContext context)
    {
        return context->m_MouseConnected;
    }

    bool IsGamepadConnected(HGamepad gamepad)
    {
        if (gamepad != 0x0)
            return gamepad->m_Connected;
        else
            return false;
    }

    bool IsTouchDeviceConnected(HContext context)
    {
        return context->m_TouchDeviceConnected;
    }

    bool IsAccelerometerConnected(HContext context)
    {
        return context->m_AccelerometerConnected;
    }

    bool GetKeyboardPacket(HContext context, KeyboardPacket* out_packet)
    {
        if (out_packet != 0x0 && context->m_KeyboardConnected)
        {
            *out_packet = context->m_KeyboardPacket;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetTextPacket(HContext context, TextPacket* out_packet)
    {
        if (out_packet != 0x0 && context->m_KeyboardConnected)
        {
            *out_packet = context->m_TextPacket;
            context->m_TextPacket.m_Size = 0;
            context->m_TextPacket.m_Text[0] = '\0';
            return true;
        }
        else
        {
            return false;
        }
    }

    void AddKeyboardChar(HContext context, int chr) {
        if (context) {
            char buf[5];
            uint32_t n = dmUtf8::ToUtf8((uint16_t) chr, buf);
            buf[n] = '\0';
            TextPacket* p = &context->m_TextPacket;
            p->m_Size = dmStrlCat(p->m_Text, buf, sizeof(p->m_Text));
        }
    }

    bool GetMarkedTextPacket(HContext context, MarkedTextPacket* out_packet)
    {
        if (out_packet != 0x0 && context->m_KeyboardConnected)
        {
            *out_packet = context->m_MarkedTextPacket;
            context->m_MarkedTextPacket.m_Size = 0;
            context->m_MarkedTextPacket.m_HasText = 0;
            context->m_MarkedTextPacket.m_Text[0] = '\0';
            return true;
        }
        else
        {
            return false;
        }
    }

    void SetMarkedText(HContext context, char* text) {
        if (context) {
            MarkedTextPacket* p = &context->m_MarkedTextPacket;
            p->m_HasText = 1;
            p->m_Size = dmStrlCpy(p->m_Text, text, sizeof(p->m_Text));
        }
    }

    void SetGamepadConnectivity(HContext context, int gamepad, bool connected) {
        assert(context);

        GamepadPacket* p = &context->m_Gamepads[gamepad].m_Packet;
        p->m_GamepadDisconnected = !connected;
        p->m_GamepadConnected = connected;
    }

    bool GetMousePacket(HContext context, MousePacket* out_packet)
    {
        if (out_packet != 0x0 && context->m_MouseConnected)
        {
            *out_packet = context->m_MousePacket;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetGamepadPacket(HGamepad gamepad, GamepadPacket* out_packet)
    {
        if (gamepad != 0x0 && out_packet != 0x0)
        {
            *out_packet = gamepad->m_Packet;
            gamepad->m_Packet.m_GamepadDisconnected = false;
            gamepad->m_Packet.m_GamepadConnected = false;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetTouchDevicePacket(HContext context, TouchDevicePacket* out_packet)
    {
        if (out_packet != 0x0 && context->m_TouchDeviceConnected)
        {
            *out_packet = context->m_TouchDevicePacket;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetAccelerationPacket(HContext context, AccelerationPacket* out_packet)
    {
        if (out_packet != 0x0)
        {
            *out_packet = context->m_AccelerationPacket;
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

    void SetKey(HContext context, Key key, bool value)
    {
        if (context != 0x0)
        {
            if (value)
                context->m_KeyboardPacket.m_Keys[key / 32] |= (1 << (key % 32));
            else
                context->m_KeyboardPacket.m_Keys[key / 32] &= ~(1 << (key % 32));
        }
    }

    bool GetMouseButton(MousePacket* packet, MouseButton button)
    {
        if (packet != 0x0)
            return packet->m_Buttons[button / 32] & (1 << (button % 32));
        else
            return false;
    }

    void SetMouseButton(HContext context, MouseButton button, bool value)
    {
        if (context != 0x0)
        {
            if (value)
                context->m_MousePacket.m_Buttons[button / 32] |= (1 << (button % 32));
            else
                context->m_MousePacket.m_Buttons[button / 32] &= ~(1 << (button % 32));
        }
    }

    void SetMousePosition(HContext context, int32_t x, int32_t y)
    {
        if (context != 0x0)
        {
            MousePacket& packet = context->m_MousePacket;
            packet.m_PositionX = x;
            packet.m_PositionY = y;
        }
    }

    void SetMouseWheel(HContext context, int32_t value)
    {
        if (context != 0x0)
        {
            context->m_MousePacket.m_Wheel = value;
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

    bool GetGamepadHat(GamepadPacket* packet, uint32_t hat, uint8_t& hat_value)
    {
        if (packet != 0x0)
        {
            hat_value = packet->m_Hat[hat];
            return true;
        } else {
            return false;
        }
    }

    void SetGamepadAxis(HGamepad gamepad, uint32_t axis, float value)
    {
        if (gamepad != 0x0)
        {
            gamepad->m_Packet.m_Axis[axis] = value;
        }
    }

    // NOTE: A bit contrived function only used for unit-tests. See AddTouchPosition
    bool GetTouch(TouchDevicePacket* packet, uint32_t touch_index, int32_t* x, int32_t* y, uint32_t* id, bool* pressed, bool* released)
    {
        if (packet != 0x0
                && x != 0x0
                && y != 0x0
                && id != 0x0)
        {
            if (touch_index < packet->m_TouchCount)
            {
                const Touch& t = packet->m_Touches[touch_index];
                *x = t.m_X;
                *y = t.m_Y;
                *id = t.m_Id;

                if (pressed != 0x0) {
                    *pressed = t.m_Phase == dmHID::PHASE_BEGAN;
                }
                if (released != 0x0) {
                    *released = t.m_Phase == dmHID::PHASE_ENDED || t.m_Phase == dmHID::PHASE_CANCELLED;
                }
                return true;
            }
        }
        return false;
    }

    // NOTE: A bit contrived function only used for unit-tests
    // We should perhaps include additional relevant touch-arguments
    void AddTouch(HContext context, int32_t x, int32_t y, uint32_t id, Phase phase)
    {
        if (context->m_TouchDeviceConnected)
        {
            TouchDevicePacket& packet = context->m_TouchDevicePacket;
            if (packet.m_TouchCount < MAX_TOUCH_COUNT)
            {
                Touch& t = packet.m_Touches[packet.m_TouchCount++];
                t.m_X = x;
                t.m_Y = y;
                t.m_Id = id;
                t.m_Phase = phase;
            }
        }
    }

    void ClearTouches(HContext context)
    {
        if (context->m_TouchDeviceConnected)
        {
            context->m_TouchDevicePacket.m_TouchCount = 0;
        }
    }
}
