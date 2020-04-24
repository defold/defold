#include "hid.h"

#include <assert.h>
#include <string.h>

#include <dlib/log.h>
#include <dlib/utf8.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>

#include "hid_private.h"

#include <nn/hid.h>
#include <nn/hid/hid_Npad.h>
#include <nn/hid/hid_NpadJoy.h>
#include <nn/hid/hid_KeyboardKey.h>

// NPad input:
// file:///C:/Nintendo/SDK/NintendoSDK/Documents/Package/contents/Pages/Page_124018876.html

// HidNPadSimple:
// file:///C:/Nintendo/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/structnn_1_1hid_1_1_npad_joy_dual_state.html&highlighttext=NpadButtonSet#a20f30af87a2ff958638de04d8f4a59b1

namespace dmHID
{
    struct NativeContext
    {
        nn::hid::TouchScreenState<nn::hid::TouchStateCountMax> m_PrevTouchState;
        dmHID::Touch    m_SingleTouch;
        uint8_t         m_SingleTouchPressed:1;
        uint8_t         m_SingleTouchReleased:1;
    };

    struct NativePadInfo {
        nn::hid::NpadIdType m_Id;
        uint8_t             m_IsLeft:1;
        uint8_t             m_IsRight:1;
        uint8_t             m_IsHorizontal:1;
        uint8_t             m_IsConnected:1;
    };

    static const char* g_GamePadNames[] = {
        "Joy-Con (Handheld)",
        "Switch Pro",
        "Joy-Con (L)",
        "Joy-Con (R)",
        "Joy-Con (Dual)",
    };

    static NativePadInfo g_NPads[MAX_GAMEPAD_COUNT] = {
        { nn::hid::NpadId::Handheld,0 },
        { nn::hid::NpadId::No1,     0 },
        { nn::hid::NpadId::No2,     0 },
        { nn::hid::NpadId::No3,     0 },
        { nn::hid::NpadId::No4,     0 },
        { nn::hid::NpadId::No5,     0 },
        { nn::hid::NpadId::No6,     0 },
        { nn::hid::NpadId::No7,     0 },
        { nn::hid::NpadId::No8,     0 },
    };

    static const int g_GamepadToNpadButton[] =
    {
        nn::hid::NpadButton::A().Index,             // 0
        nn::hid::NpadButton::B().Index,
        nn::hid::NpadButton::Y().Index,
        nn::hid::NpadButton::X().Index,
        nn::hid::NpadButton::Right().Index,
        nn::hid::NpadButton::Down().Index,          // 5
        nn::hid::NpadButton::Left().Index,
        nn::hid::NpadButton::Up().Index,
        nn::hid::NpadButton::StickL().Index,
        nn::hid::NpadButton::StickLRight().Index,
        nn::hid::NpadButton::StickLDown().Index,    // 10
        nn::hid::NpadButton::StickLLeft().Index,
        nn::hid::NpadButton::StickLUp().Index,
        nn::hid::NpadButton::StickR().Index,
        nn::hid::NpadButton::StickRRight().Index,
        nn::hid::NpadButton::StickRDown().Index,    // 15
        nn::hid::NpadButton::StickRLeft().Index,
        nn::hid::NpadButton::StickRUp().Index,
        nn::hid::NpadButton::L().Index,
        nn::hid::NpadButton::R().Index,
        nn::hid::NpadButton::ZL().Index,            // 20
        nn::hid::NpadButton::ZR().Index,
        nn::hid::NpadButton::Minus().Index,
        nn::hid::NpadButton::Plus().Index,
    };


/*
    // TODO: Unfortunately a global variable as glfw has no notion of user-data context
    HContext g_Context = 0;
    static void CharacterCallback(int chr,int)
    {
        AddKeyboardChar(g_Context, chr);
    }

    static void MarkedTextCallback(char* text)
    {
        SetMarkedText(g_Context, text);
    }
*/
    static void GamepadCallback(HContext context, int gamepad_id, int connected)
    {
        if (context->m_GamepadConnectivityCallback) {
            context->m_GamepadConnectivityCallback(gamepad_id, connected, context->m_GamepadConnectivityUserdata);
        }
        SetGamepadConnectivity(context, gamepad_id, connected);
    }

    bool Init(HContext context)
    {
        assert(MAX_GAMEPAD_COUNT == HID_NATIVE_MAX_GAMEPAD_COUNT);
        if (context != 0x0)
        {
            if (!context->m_NativeContext)
            {
                NativeContext* native_context = new NativeContext;
                memset(native_context, 0, sizeof(NativeContext));
                context->m_NativeContext = (void*)native_context;
            }

            nn::hid::InitializeNpad();

            nn::hid::NpadIdType ids[] = {
                nn::hid::NpadId::Handheld, nn::hid::NpadId::No1, nn::hid::NpadId::No2, nn::hid::NpadId::No3, nn::hid::NpadId::No4,
                nn::hid::NpadId::No5, nn::hid::NpadId::No6, nn::hid::NpadId::No7, nn::hid::NpadId::No8,
            };
            nn::hid::SetSupportedNpadIdType(ids, MAX_GAMEPAD_COUNT);

            // TODO: This list should be configurable
            nn::hid::SetSupportedNpadStyleSet(
                nn::hid::NpadStyleFullKey::Mask |
                nn::hid::NpadStyleJoyDual::Mask |
                nn::hid::NpadStyleHandheld::Mask|
                nn::hid::NpadStyleJoyLeft::Mask | 
                nn::hid::NpadStyleJoyRight::Mask);

            // TODO: This call should be configurable or callable from Lua
            nn::hid::SetNpadJoyHoldType(nn::hid::NpadJoyHoldType::NpadJoyHoldType_Horizontal);

            if (!context->m_IgnoreTouchDevice)
            {
                nn::hid::InitializeTouchScreen();
            }

            context->m_KeyboardConnected = 0;
            context->m_MouseConnected = 0;
            context->m_TouchDeviceConnected = 0;
            for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
            {
                Gamepad& gamepad = context->m_Gamepads[i];
                memset(&gamepad, 0, sizeof(GamepadPacket));
                gamepad.m_Index = i;
            }
            return true;
        }
        return false;
    }

    void Final(HContext context)
    {
        NativeContext* native_context = (NativeContext*)context->m_NativeContext;
        delete native_context;
        context->m_NativeContext = 0;
    }

    // NpadFullKeyState: file:///C:/Nintendo/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/structnn_1_1hid_1_1_npad_full_key_state.html&highlighttext=NpadFullKeyState
    static void UpdateGamepad(HContext context, uint32_t gamepad_id, nn::hid::NpadButtonSet* buttons, nn::hid::AnalogStickState* stickL, nn::hid::AnalogStickState* stickR, bool connected)
    {
        Gamepad* pad = &context->m_Gamepads[gamepad_id];
        pad->m_AxisCount = 4; // left (X/Y), right (X/Y)

        GamepadPacket* packet = &pad->m_Packet;
        packet->m_Axis[0] = stickL->x / (float)nn::hid::AnalogStickMax;
        packet->m_Axis[1] = stickL->y / (float)nn::hid::AnalogStickMax;
        packet->m_Axis[2] = stickR->x / (float)nn::hid::AnalogStickMax;
        packet->m_Axis[3] = stickR->y / (float)nn::hid::AnalogStickMax;

        // Buttons: file:///C:/Nintendo/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/structnn_1_1hid_1_1_npad_full_key_state.html&highlighttext=NpadFullKeyState
        pad->m_HatCount = 0;

        // different joy styles:
        // file:///C:/Nintendo/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/structnn_1_1hid_1_1_npad_joy_dual_state.html&highlighttext=NpadButtonSet#a20f30af87a2ff958638de04d8f4a59b1

        pad->m_ButtonCount = sizeof(g_GamepadToNpadButton)/sizeof(g_GamepadToNpadButton[0]);
        for (int i = 0; i < pad->m_ButtonCount; ++i)
        {
            int button_flag_index = g_GamepadToNpadButton[i];
            if (buttons->Test(button_flag_index))
            {
                packet->m_Buttons[i / 32] |= 1 << (i % 32);
            }
            else
            {
                packet->m_Buttons[i / 32] &= ~(1 << (i % 32));
            }
        }
    }

    static void UpdateConnectivity(HContext context, uint32_t gamepad_id, bool connected)
    {
        Gamepad* pad = &context->m_Gamepads[gamepad_id];

        uint8_t was_connected = g_NPads[pad->m_Index].m_IsConnected;
        g_NPads[pad->m_Index].m_IsConnected = connected ? 1 : 0;

        pad->m_Connected = connected;

        if (was_connected != g_NPads[pad->m_Index].m_IsConnected) {
            GamepadCallback(context, gamepad_id, pad->m_Connected);
        }
    }

    static nn::hid::TouchState* FindFromFingerId(int count, nn::hid::TouchState* touches, int finger_id)
    {
        for (int i = 0; i < count; ++i)
        {
            if (touches[i].fingerId == finger_id)
                return &touches[i];
        }
        return 0;
    }

    static dmHID::Phase CalcTouchPhase(nn::hid::TouchState* current, nn::hid::TouchState* prev)
    {
        if (!prev || current->attributes.Test<nn::hid::TouchAttribute::Start>())
        {
            return dmHID::PHASE_BEGAN;
        }
        if (current->attributes.Test<nn::hid::TouchAttribute::End>())
        {
            return dmHID::PHASE_ENDED;
        }

        assert(prev != 0);
        if (prev->x == current->x && prev->y == current->y)
            return dmHID::PHASE_STATIONARY;
        else
            return dmHID::PHASE_MOVED;
    }

    // From %NintendoSDK%\Samples\Sources\Applications\HidControllerSequence\HidControllerSequence_ControllerSupportApplet.cpp
    static nn::hid::NpadButtonSet ConvertHorizontalButtonLayout(const nn::hid::NpadStyleSet& style, const nn::hid::NpadButtonSet& button)
    {
        nn::hid::NpadButtonSet convButton = button;
        if (nn::hid::GetNpadJoyHoldType() == nn::hid::NpadJoyHoldType_Horizontal)
        {
            convButton.Reset();
            if (style.Test<nn::hid::NpadStyleJoyLeft>())
            {
                convButton.Set<nn::hid::NpadButton::A>(button.Test<nn::hid::NpadButton::Down>());
                convButton.Set<nn::hid::NpadButton::B>(button.Test<nn::hid::NpadButton::Left>());
                convButton.Set<nn::hid::NpadButton::X>(button.Test<nn::hid::NpadButton::Right>());
                convButton.Set<nn::hid::NpadButton::Y>(button.Test<nn::hid::NpadButton::Up>());

                convButton.Set<nn::hid::NpadButton::StickLRight>(button.Test<nn::hid::NpadButton::StickLDown>());
                convButton.Set<nn::hid::NpadButton::StickLDown>(button.Test<nn::hid::NpadButton::StickLLeft>());
                convButton.Set<nn::hid::NpadButton::StickLUp>(button.Test<nn::hid::NpadButton::StickLRight>());
                convButton.Set<nn::hid::NpadButton::StickLLeft>(button.Test<nn::hid::NpadButton::StickLUp>());
                convButton.Set<nn::hid::NpadButton::StickL>(button.Test<nn::hid::NpadButton::StickL>());

                convButton.Set<nn::hid::NpadButton::L>(button.Test<nn::hid::NpadJoyButton::LeftSL>());
                convButton.Set<nn::hid::NpadButton::R>(button.Test<nn::hid::NpadJoyButton::LeftSR>());

                convButton.Set<nn::hid::NpadButton::Minus>(button.Test<nn::hid::NpadButton::Minus>());
            }
            else if (style.Test<nn::hid::NpadStyleJoyRight>())
            {
                convButton.Set<nn::hid::NpadButton::A>(button.Test<nn::hid::NpadButton::X>());
                convButton.Set<nn::hid::NpadButton::B>(button.Test<nn::hid::NpadButton::A>());
                convButton.Set<nn::hid::NpadButton::X>(button.Test<nn::hid::NpadButton::Y>());
                convButton.Set<nn::hid::NpadButton::Y>(button.Test<nn::hid::NpadButton::B>());

                convButton.Set<nn::hid::NpadButton::StickRRight>(button.Test<nn::hid::NpadButton::StickRUp>());
                convButton.Set<nn::hid::NpadButton::StickRDown>(button.Test<nn::hid::NpadButton::StickRRight>());
                convButton.Set<nn::hid::NpadButton::StickRUp>(button.Test<nn::hid::NpadButton::StickRLeft>());
                convButton.Set<nn::hid::NpadButton::StickRLeft>(button.Test<nn::hid::NpadButton::StickRDown>());
                convButton.Set<nn::hid::NpadButton::StickR>(button.Test<nn::hid::NpadButton::StickR>());

                convButton.Set<nn::hid::NpadButton::L>(button.Test<nn::hid::NpadJoyButton::RightSL>());
                convButton.Set<nn::hid::NpadButton::R>(button.Test<nn::hid::NpadJoyButton::RightSR>());

                convButton.Set<nn::hid::NpadButton::Plus>(button.Test<nn::hid::NpadButton::Plus>());
            }
        }
        return convButton;
    }

    static nn::hid::AnalogStickState ConvertHorizontalAnalogStick(const nn::hid::NpadStyleSet& style, const nn::hid::AnalogStickState& analogStick)
    {
        nn::hid::AnalogStickState convStickAxis = analogStick;
        if (nn::hid::GetNpadJoyHoldType() == nn::hid::NpadJoyHoldType_Horizontal)
        {
            if (style.Test<nn::hid::NpadStyleJoyLeft>())
            {
                convStickAxis.y = analogStick.x;
                convStickAxis.x = -analogStick.y;
            }
            else if (style.Test<nn::hid::NpadStyleJoyRight>())
            {
                convStickAxis.y = -analogStick.x;
                convStickAxis.x = analogStick.y;
            }
        }
        return convStickAxis;
    }

    void Update(HContext context)
    {
        NativeContext* native_context = (NativeContext*)context->m_NativeContext;

        if (!context->m_IgnoreGamepads)
        {
            for (int i = 0; i < MAX_GAMEPAD_COUNT; ++i)
            {
                nn::hid::NpadStyleSet style = nn::hid::GetNpadStyleSet(g_NPads[i].m_Id);
                nn::hid::NpadButtonSet buttons;
                nn::hid::AnalogStickState stickL = {0};
                nn::hid::AnalogStickState stickR = {0};
                uint8_t left;
                uint8_t right;
                bool connected = false;

                if (style.Test<nn::hid::NpadStyleFullKey>()) // Switch Pro
                {
                    nn::hid::NpadFullKeyState state;
                    nn::hid::GetNpadState(&state, g_NPads[i].m_Id);
                    connected = state.attributes.Test<nn::hid::NpadAttribute::IsConnected>();
                    buttons  = ConvertHorizontalButtonLayout(style, state.buttons);
                    stickL   = state.analogStickL;
                    stickR   = state.analogStickR;
                    left = right = 0;
                    g_NPads[i].m_IsRight = g_NPads[i].m_IsLeft = 0;

                    // The pro controller has accelerometer and gyro
                    // It has two rumble motors
                }
                else if (style.Test<nn::hid::NpadStyleJoyDual>()) // Dual Mode
                {
                    nn::hid::NpadJoyDualState state;
                    nn::hid::GetNpadState(&state, g_NPads[i].m_Id);
                    connected = state.attributes.Test<nn::hid::NpadAttribute::IsConnected>();
                    buttons  = ConvertHorizontalButtonLayout(style, state.buttons);
                    stickL   = state.analogStickL;
                    stickR   = state.analogStickR;
                    left = right = 1;
                    // It has two rumble motors
                }
                else if (style.Test<nn::hid::NpadStyleHandheld>()) // Handheld mode
                {
                    nn::hid::NpadHandheldState state;
                    nn::hid::GetNpadState(&state, g_NPads[i].m_Id);
                    connected = state.attributes.Test<nn::hid::NpadAttribute::IsConnected>();
                    buttons  = ConvertHorizontalButtonLayout(style, state.buttons);
                    stickL   = state.analogStickL;
                    stickR   = state.analogStickR;
                    left = right = 0;
                }
                else if (style.Test<nn::hid::NpadStyleJoyLeft>()) // JoyCon (L)
                {
                    nn::hid::NpadJoyLeftState state;
                    nn::hid::GetNpadState(&state, g_NPads[i].m_Id);
                    connected = state.attributes.Test<nn::hid::NpadAttribute::IsConnected>();
                    buttons  = ConvertHorizontalButtonLayout(style, state.buttons);
                    stickL   = ConvertHorizontalAnalogStick(style, state.analogStickL);
                    left = 1;
                    right = 0;
                }
                else if (style.Test<nn::hid::NpadStyleJoyRight>()) // JoyCon (R)
                {
                    nn::hid::NpadJoyRightState state;
                    nn::hid::GetNpadState(&state, g_NPads[i].m_Id);
                    connected = state.attributes.Test<nn::hid::NpadAttribute::IsConnected>();
                    buttons  = ConvertHorizontalButtonLayout(style, state.buttons);
                    stickR   = ConvertHorizontalAnalogStick(style, state.analogStickR);
                    left = 0;
                    right = 1;
                }

                g_NPads[i].m_IsLeft  = left;
                g_NPads[i].m_IsRight = right;
                UpdateConnectivity(context, i, connected);
                if (connected)
                    UpdateGamepad(context, i, &buttons, &stickL, &stickR, connected);
            }

            // TODO: nn::hid::GetNpadControllerColor
        }

        if (!context->m_IgnoreTouchDevice)
        {
            context->m_TouchDeviceConnected = 1;

            nn::hid::TouchScreenState<nn::hid::TouchStateCountMax> state;
            nn::hid::GetTouchScreenState(&state);

            TouchDevicePacket* packet = &context->m_TouchDevicePacket;
            packet->m_TouchCount = state.count;

            nn::hid::TouchScreenState<nn::hid::TouchStateCountMax>* prev_state = &native_context->m_PrevTouchState;

            for (int i = 0; i < state.count; ++i)
            {
                nn::hid::TouchState* touch = &state.touches[i];
                nn::hid::TouchState* prev_touch = FindFromFingerId(prev_state->count, prev_state->touches, touch->fingerId);

                packet->m_Touches[i].m_TapCount = 0;//glfw_touch[i].TapCount;
                packet->m_Touches[i].m_Id = touch->fingerId;
                packet->m_Touches[i].m_Phase = CalcTouchPhase(touch, prev_touch);
                packet->m_Touches[i].m_X = touch->x;
                packet->m_Touches[i].m_Y = touch->y;
                packet->m_Touches[i].m_DX = prev_touch ? (touch->x - prev_touch->x) : 0;
                packet->m_Touches[i].m_DY = prev_touch ? (touch->y - prev_touch->y) : 0;

                if (prev_state->count == 0 && !native_context->m_SingleTouchPressed)
                {
                    // First frame
                    native_context->m_SingleTouch = packet->m_Touches[i];
                    native_context->m_SingleTouchPressed = 1;
                }
                else if (native_context->m_SingleTouchPressed)
                {
                    // Intermediate frames
                    nn::hid::TouchState* t = FindFromFingerId(state.count, state.touches, native_context->m_SingleTouch.m_Id);
                    if (t)
                    {
                        native_context->m_SingleTouch.m_DX = native_context->m_SingleTouch.m_X - t->x;
                        native_context->m_SingleTouch.m_DY = native_context->m_SingleTouch.m_Y - t->y;
                        native_context->m_SingleTouch.m_X = t->x;
                        native_context->m_SingleTouch.m_Y = t->y;
                    }
                }

/*
                printf("packet: f: %d  x/y: %d, %d  dx/dy: %d, %d  phase: %d   prev: %p  x/y: %d, %d\n", packet->m_Touches[i].m_Id,
                        packet->m_Touches[i].m_X, packet->m_Touches[i].m_Y,
                        packet->m_Touches[i].m_DX, packet->m_Touches[i].m_DY,
                        packet->m_Touches[i].m_Phase,
                        prev_touch,
                        prev_touch ? prev_touch->x : -1, prev_touch ? prev_touch->y : -1
                        );
                */
            }

            // Since we don't get an event when a finger is released
            for (int pi = 0; pi < prev_state->count; ++pi)
            {
                int i = state.count + pi;

                nn::hid::TouchState* prev_touch = &prev_state->touches[pi];
                nn::hid::TouchState* touch = FindFromFingerId(state.count, state.touches, prev_touch->fingerId);

                if (touch)
                    continue;

                // We found a touch that was removed

                packet->m_Touches[i].m_TapCount = 0;//glfw_touch[i].TapCount;
                packet->m_Touches[i].m_Id = prev_touch->fingerId;
                packet->m_Touches[i].m_Phase = dmHID::PHASE_ENDED;
                packet->m_Touches[i].m_X = prev_touch->x;
                packet->m_Touches[i].m_Y = prev_touch->y;
                packet->m_Touches[i].m_DX = 0;  // TODO: fix the delta from the previous known position
                packet->m_Touches[i].m_DY = 0;

                if (prev_touch->fingerId == native_context->m_SingleTouch.m_Id) {
                    native_context->m_SingleTouchPressed = 0;
                    native_context->m_SingleTouchReleased = 1;
                }

/*
                printf("packet: f: %d  x/y: %d, %d  dx/dy: %d, %d  phase: %d\n", packet->m_Touches[i].m_Id,
                        packet->m_Touches[i].m_X, packet->m_Touches[i].m_Y,
                        packet->m_Touches[i].m_DX, packet->m_Touches[i].m_DY,
                        packet->m_Touches[i].m_Phase
                        );
                */
            }


            memcpy(&native_context->m_PrevTouchState, &state, sizeof(state));
        }

        // Update mouse (since the single "touch" event is generated as a mouse event)
        if (!context->m_IgnoreMouse)
        {
            context->m_MouseConnected = 1;
            MousePacket& packet = context->m_MousePacket;

            if (native_context->m_SingleTouchPressed || native_context->m_SingleTouchReleased)
            {
                int i = dmHID::MOUSE_BUTTON_LEFT;
                if (native_context->m_SingleTouchPressed)
                    packet.m_Buttons[i / 32] |= 1 << (i % 32);
                else
                    packet.m_Buttons[i / 32] &= ~(1 << (i % 32));

                packet.m_PositionX = native_context->m_SingleTouch.m_X;
                packet.m_PositionY = native_context->m_SingleTouch.m_Y;
                native_context->m_SingleTouchReleased = 0;
            }
            /*
            for (uint32_t i = 0; i < MAX_MOUSE_BUTTON_COUNT; ++i)
            {
                uint32_t mask = 1;
                mask <<= i % 32;
                int button = glfwGetMouseButton(i);
                if (button == GLFW_PRESS)
                    packet.m_Buttons[i / 32] |= mask;
                else
                    packet.m_Buttons[i / 32] &= ~mask;
            }
            int32_t wheel = glfwGetMouseWheel();
            if (context->m_FlipScrollDirection)
            {
                wheel *= -1;
            }
            packet.m_Wheel = wheel;
            glfwGetMousePos(&packet.m_PositionX, &packet.m_PositionY);
            */
        }


        // Update keyboard
        /*
        if (!context->m_IgnoreKeyboard)
        {
            // TODO: Actually detect if the keyboard is present
            context->m_KeyboardConnected = 1;
            for (uint32_t i = 0; i < MAX_KEY_COUNT; ++i)
            {
                uint32_t mask = 1;
                mask <<= i % 32;
                int key = glfwGetKey(i);
                if (key == GLFW_PRESS)
                    context->m_KeyboardPacket.m_Keys[i / 32] |= mask;
                else
                    context->m_KeyboardPacket.m_Keys[i / 32] &= ~mask;
            }
        }

        if (!context->m_IgnoreAcceleration)
        {
            AccelerationPacket packet;
            context->m_AccelerometerConnected = 0;
            if (glfwGetAcceleration(&packet.m_X, &packet.m_Y, &packet.m_Z))
            {
                context->m_AccelerometerConnected = 1;
                context->m_AccelerationPacket = packet;
            }
        }
        */
    }

    void GetGamepadDeviceName(HGamepad gamepad, const char** device_name)
    {
        if (g_NPads[gamepad->m_Index].m_Id == nn::hid::NpadId::Handheld) {
            *device_name = g_GamePadNames[0];
        }
        else {
            uint8_t left = g_NPads[gamepad->m_Index].m_IsLeft;
            uint8_t right = g_NPads[gamepad->m_Index].m_IsRight << 1;
            *device_name = g_GamePadNames[1 + (left|right)];
        }
    }

    void ShowKeyboard(HContext context, KeyboardType type, bool autoclose)
    {
        dmLogWarning("%s not implemented yet\n", __FUNCTION__);
        /*
        int t = GLFW_KEYBOARD_DEFAULT;
        switch (type) {
            case KEYBOARD_TYPE_DEFAULT:
                t = GLFW_KEYBOARD_DEFAULT;
                break;
            case KEYBOARD_TYPE_NUMBER_PAD:
                t = GLFW_KEYBOARD_NUMBER_PAD;
                break;
            case KEYBOARD_TYPE_EMAIL:
                t = GLFW_KEYBOARD_EMAIL;
                break;
            case KEYBOARD_TYPE_PASSWORD:
                t = GLFW_KEYBOARD_PASSWORD;
                break;
            default:
                dmLogWarning("Unknown keyboard type %d\n", type);
        }
        glfwShowKeyboard(1, t, (int) autoclose);
        */
    }

    void HideKeyboard(HContext context)
    {
        dmLogWarning("%s not implemented yet\n", __FUNCTION__);
        //glfwShowKeyboard(0, GLFW_KEYBOARD_DEFAULT, 0);
    }

    void ResetKeyboard(HContext context)
    {
        dmLogWarning("%s not implemented yet\n", __FUNCTION__);
        //glfwResetKeyboard();
    }

    void EnableAccelerometer()
    {
        dmLogWarning("%s not implemented yet\n", __FUNCTION__);
        //glfwAccelerometerEnable();
    }

    const char* GetKeyName(Key key)
    {
        dmLogWarning("%s not implemented yet\n", __FUNCTION__);
        return "no name";
    }

    const char* GetMouseButtonName(MouseButton input)
    {
        dmLogWarning("%s not implemented yet\n", __FUNCTION__);
        return "no name";
    }
}
