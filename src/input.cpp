#include "input.h"

#include <string.h>
#include <stdio.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/platform.h>
#include <dlib/profile.h>

#include "input_private.h"

namespace dmInput
{
    dmHID::Key KEY_MAP[dmInputDDF::MAX_KEY_COUNT];
    dmHID::MouseButton MOUSE_BUTTON_MAP[dmInputDDF::MAX_KEY_COUNT];

    void InitKeyMap();
    void InitMouseButtonMap();
    bool g_Init = false;

    HContext NewContext(float repeat_delay, float repeat_interval)
    {
        if (!g_Init)
        {
            InitKeyMap();
            InitMouseButtonMap();
            g_Init = true;
        }
        Context* context = new Context();
        context->m_GamepadIndices.SetCapacity(16);
        context->m_GamepadMaps.SetCapacity(8, 16);
        context->m_RepeatDelay = repeat_delay;
        context->m_RepeatInterval = repeat_interval;
        return context;
    }

    void DeleteContext(HContext context)
    {
        delete context;
    }

    void SetRepeat(HContext context, float delay, float interval)
    {
        context->m_RepeatDelay = delay;
        context->m_RepeatInterval = interval;
    }

    HBinding NewBinding(HContext context, dmInputDDF::InputBinding* ddf)
    {
        Binding* binding = new Binding();
        memset(binding, 0, sizeof(Binding));
        binding->m_Context = context;
        binding->m_Actions.SetCapacity(64, 256);
        Action action;
        memset(&action, 0, sizeof(Action));
        if (ddf->m_KeyTrigger.m_Count > 0)
        {
            binding->m_KeyboardBinding = new KeyboardBinding();
            memset(binding->m_KeyboardBinding, 0, sizeof(*binding->m_KeyboardBinding));
            binding->m_KeyboardBinding->m_Triggers.SetCapacity(ddf->m_KeyTrigger.m_Count);
            for (uint32_t i = 0; i < ddf->m_KeyTrigger.m_Count; ++i)
            {
                const dmInputDDF::KeyTrigger& ddf_trigger = ddf->m_KeyTrigger[i];
                dmInput::KeyTrigger trigger;
                trigger.m_ActionId = dmHashString64(ddf_trigger.m_Action);
                trigger.m_Input = ddf_trigger.m_Input;
                binding->m_KeyboardBinding->m_Triggers.Push(trigger);
                binding->m_Actions.Put(trigger.m_ActionId, action);
            }
        }
        if (ddf->m_MouseTrigger.m_Count > 0)
        {
            binding->m_MouseBinding = new MouseBinding();
            memset(binding->m_MouseBinding, 0, sizeof(*binding->m_MouseBinding));
            binding->m_MouseBinding->m_Triggers.SetCapacity(ddf->m_MouseTrigger.m_Count);
            for (uint32_t i = 0; i < ddf->m_MouseTrigger.m_Count; ++i)
            {
                const dmInputDDF::MouseTrigger& ddf_trigger = ddf->m_MouseTrigger[i];
                dmInput::MouseTrigger trigger;
                trigger.m_ActionId = dmHashString64(ddf_trigger.m_Action);
                trigger.m_Input = ddf_trigger.m_Input;
                binding->m_MouseBinding->m_Triggers.Push(trigger);
                binding->m_Actions.Put(trigger.m_ActionId, action);
            }
        }
        if (ddf->m_GamepadTrigger.m_Count > 0)
        {
            uint8_t gamepad_index = context->m_GamepadIndices.Pop();
            dmHID::HGamepad gamepad = dmHID::GetGamepad(gamepad_index);
            const char* device_name = 0x0;
            dmHID::GetGamepadDeviceName(gamepad, &device_name);
            if (device_name == 0x0)
            {
                dmLogWarning("Gamepad %d is not connected.", gamepad_index);
            }
            else
            {
                GamepadConfig* config = context->m_GamepadMaps.Get(dmHashString32(device_name));
                if (config == 0x0)
                {
                    dmLogWarning("No gamepad map found for gamepad %d (%s), it will not be used.", gamepad_index, device_name);
                }
            }
            binding->m_GamepadBinding = new GamepadBinding();
            memset(binding->m_GamepadBinding, 0, sizeof(*binding->m_GamepadBinding));
            binding->m_GamepadBinding->m_Gamepad = gamepad;
            binding->m_GamepadBinding->m_Index = gamepad_index;
            binding->m_GamepadBinding->m_Triggers.SetCapacity(ddf->m_GamepadTrigger.m_Count);
            for (uint32_t i = 0; i < ddf->m_GamepadTrigger.m_Count; ++i)
            {
                const dmInputDDF::GamepadTrigger& ddf_trigger = ddf->m_GamepadTrigger[i];
                dmInput::GamepadTrigger trigger;
                trigger.m_ActionId = dmHashString64(ddf_trigger.m_Action);
                trigger.m_Input = ddf_trigger.m_Input;
                binding->m_GamepadBinding->m_Triggers.Push(trigger);
                binding->m_Actions.Put(trigger.m_ActionId, action);
            }
        }
        return binding;
    }

    void DeleteBinding(HBinding binding)
    {
        if (binding->m_KeyboardBinding != 0x0)
            delete binding->m_KeyboardBinding;
        if (binding->m_MouseBinding != 0x0)
            delete binding->m_MouseBinding;
        if (binding->m_GamepadBinding != 0x0)
        {
            binding->m_Context->m_GamepadIndices.Push(binding->m_GamepadBinding->m_Index);
            delete binding->m_GamepadBinding;
        }
        delete binding;
    }

    void RegisterGamepads(HContext context, const dmInputDDF::GamepadMaps* ddf)
    {
        for (uint32_t i = 0; i < ddf->m_Driver.m_Count; ++i)
        {
            const dmInputDDF::GamepadMap& gamepad_map = ddf->m_Driver[i];
            if (strcmp(DM_PLATFORM, gamepad_map.m_Platform) == 0)
            {
                uint32_t device_id = dmHashString32(gamepad_map.m_Device);
                if (context->m_GamepadMaps.Get(device_id) == 0x0)
                {
                    GamepadConfig config;
                    config.m_DeadZone = gamepad_map.m_DeadZone;
                    memset(config.m_Inputs, 0, sizeof(config.m_Inputs));
                    for (uint32_t j = 0; j < gamepad_map.m_Map.m_Count; ++j)
                    {
                        const dmInputDDF::GamepadMapEntry& entry = gamepad_map.m_Map[j];
                        GamepadInput& input = config.m_Inputs[entry.m_Input];
                        input.m_Index = (uint16_t)entry.m_Index;
                        input.m_Type = entry.m_Type;
                        for (uint32_t k = 0; k < entry.m_Mod.m_Count; ++k)
                        {
                            switch (entry.m_Mod[k].m_Mod)
                            {
                            case dmInputDDF::GAMEPAD_MODIFIER_CLAMP:
                                input.m_Clamp = 1;
                                break;
                            case dmInputDDF::GAMEPAD_MODIFIER_NEGATE:
                                input.m_Negate = 1;
                                break;
                            case dmInputDDF::GAMEPAD_MODIFIER_SCALE:
                                input.m_Scale = 1;
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    context->m_GamepadMaps.Put(device_id, config);
                }
                else
                {
                    dmLogWarning("Gamepad map for device '%s' already registered.", ddf->m_Driver[i].m_Device);
                }
            }
        }
    }

    float ApplyGamepadModifiers(dmHID::GamepadPacket* packet, const GamepadInput& input);

    void ClearAction(void*, const dmhash_t* id, Action* action)
    {
        action->m_PrevValue = action->m_Value;
        action->m_Value = 0.0f;
    }

    struct UpdateContext
    {
        float m_DT;
        Context* m_Context;
    };

    void UpdateAction(void* context, const dmhash_t* id, Action* action)
    {
        action->m_Pressed = (action->m_PrevValue == 0.0f && action->m_Value > 0.0f) ? 1 : 0;
        action->m_Released = (action->m_PrevValue > 0.0f && action->m_Value == 0.0f) ? 1 : 0;
        action->m_Repeated = false;
        if (action->m_Value > 0.0f)
        {
            UpdateContext* update_context = (UpdateContext*)context;
            if (action->m_Pressed)
            {
                action->m_Repeated = true;
                action->m_RepeatTimer = update_context->m_Context->m_RepeatDelay;
            }
            else
            {
                action->m_RepeatTimer -= update_context->m_DT;
                if (action->m_RepeatTimer <= 0.0f)
                {
                    action->m_Repeated = true;
                    action->m_RepeatTimer += update_context->m_Context->m_RepeatInterval;
                }
            }
        }
    }

    void UpdateBinding(HBinding binding, float dt)
    {
        DM_PROFILE(Input, "UpdateContext");
        binding->m_Actions.Iterate<void>(ClearAction, 0x0);

        if (binding->m_KeyboardBinding != 0x0)
        {
            dmHID::GetKeyboardPacket(&binding->m_KeyboardBinding->m_Packet);
            for (uint32_t i = 0; i < binding->m_KeyboardBinding->m_Triggers.Size(); ++i)
            {
                const KeyTrigger& trigger = binding->m_KeyboardBinding->m_Triggers[i];
                float v = dmHID::GetKey(&binding->m_KeyboardBinding->m_Packet, KEY_MAP[trigger.m_Input]) ? 1.0f : 0.0f;
                Action* action = binding->m_Actions.Get(trigger.m_ActionId);
                if (action != 0x0)
                {
                    if (dmMath::Abs(action->m_Value) < v)
                        action->m_Value = v;
                }
            }
            binding->m_KeyboardBinding->m_PreviousPacket = binding->m_KeyboardBinding->m_Packet;
        }
        if (binding->m_MouseBinding != 0x0)
        {
            dmHID::GetMousePacket(&binding->m_MouseBinding->m_Packet);
            for (uint32_t i = 0; i < binding->m_MouseBinding->m_Triggers.Size(); ++i)
            {
                const MouseTrigger& trigger = binding->m_MouseBinding->m_Triggers[i];
                float v = 0.0f;
                switch (trigger.m_Input)
                {
                case dmInputDDF::MOUSE_RIGHT:
                    v = binding->m_MouseBinding->m_Packet.m_PositionX - binding->m_MouseBinding->m_PreviousPacket.m_PositionX;
                    break;
                case dmInputDDF::MOUSE_LEFT:
                    v = -(binding->m_MouseBinding->m_Packet.m_PositionX - binding->m_MouseBinding->m_PreviousPacket.m_PositionX);
                    break;
                case dmInputDDF::MOUSE_UP:
                    v = binding->m_MouseBinding->m_Packet.m_PositionY - binding->m_MouseBinding->m_PreviousPacket.m_PositionY;
                    break;
                case dmInputDDF::MOUSE_DOWN:
                    v = -(binding->m_MouseBinding->m_Packet.m_PositionY - binding->m_MouseBinding->m_PreviousPacket.m_PositionY);
                    break;
                case dmInputDDF::MOUSE_WHEEL_UP:
                    v = binding->m_MouseBinding->m_Packet.m_Wheel - binding->m_MouseBinding->m_PreviousPacket.m_Wheel;
                    break;
                case dmInputDDF::MOUSE_WHEEL_DOWN:
                    v = -(binding->m_MouseBinding->m_Packet.m_Wheel - binding->m_MouseBinding->m_PreviousPacket.m_Wheel);
                    break;
                default:
                    v = dmHID::GetMouseButton(&binding->m_MouseBinding->m_Packet, MOUSE_BUTTON_MAP[trigger.m_Input]) ? 1.0f : 0.0f;
                    break;
                }
                v = dmMath::Clamp(v, 0.0f, 1.0f);
                Action* action = binding->m_Actions.Get(trigger.m_ActionId);
                if (action != 0x0)
                {
                    if (dmMath::Abs(action->m_Value) < dmMath::Abs(v))
                        action->m_Value = v;
                }
            }
            binding->m_MouseBinding->m_PreviousPacket = binding->m_MouseBinding->m_Packet;
        }
        if (binding->m_GamepadBinding != 0x0)
        {
            bool connected = dmHID::IsGamepadConnected(binding->m_GamepadBinding->m_Gamepad);
            if (!binding->m_GamepadBinding->m_Connected)
            {
                if (connected)
                {
                    const char* device_name;
                    dmHID::GetGamepadDeviceName(binding->m_GamepadBinding->m_Gamepad, &device_name);
                    binding->m_GamepadBinding->m_DeviceId = dmHashString32(device_name);
                    binding->m_GamepadBinding->m_Connected = 1;
                    binding->m_GamepadBinding->m_NoMapWarning = 0;
                }
            }
            binding->m_GamepadBinding->m_Connected = connected;
            if (binding->m_GamepadBinding->m_Connected)
            {
                GamepadConfig* config = binding->m_Context->m_GamepadMaps.Get(binding->m_GamepadBinding->m_DeviceId);
                if (config != 0x0)
                {
                    dmHID::GetGamepadPacket(binding->m_GamepadBinding->m_Gamepad, &binding->m_GamepadBinding->m_Packet);

                    // Apply dead zone
                    uint32_t lstick_vert_axis = config->m_Inputs[dmInputDDF::GAMEPAD_LSTICK_UP].m_Index;
                    uint32_t lstick_hori_axis = config->m_Inputs[dmInputDDF::GAMEPAD_LSTICK_LEFT].m_Index;
                    uint32_t rstick_vert_axis = config->m_Inputs[dmInputDDF::GAMEPAD_RSTICK_UP].m_Index;
                    uint32_t rstick_hori_axis = config->m_Inputs[dmInputDDF::GAMEPAD_RSTICK_LEFT].m_Index;
                    if (lstick_vert_axis != ~0U && lstick_hori_axis != ~0U)
                    {
                        float& lx = binding->m_GamepadBinding->m_Packet.m_Axis[lstick_hori_axis];
                        float& ly = binding->m_GamepadBinding->m_Packet.m_Axis[lstick_vert_axis];
                        if (lx * lx + ly * ly <= config->m_DeadZone * config->m_DeadZone)
                        {
                            lx = 0.0f;
                            ly = 0.0f;
                        }
                    }
                    if (rstick_vert_axis != ~0U && rstick_hori_axis != ~0U)
                    {
                        float& rx = binding->m_GamepadBinding->m_Packet.m_Axis[rstick_hori_axis];
                        float& ry = binding->m_GamepadBinding->m_Packet.m_Axis[rstick_vert_axis];
                        if (rx * rx + ry * ry <= config->m_DeadZone * config->m_DeadZone)
                        {
                            rx = 0.0f;
                            ry = 0.0f;
                        }
                    }

                    for (uint32_t i = 0; i < binding->m_GamepadBinding->m_Triggers.Size(); ++i)
                    {
                        const GamepadTrigger& trigger = binding->m_GamepadBinding->m_Triggers[i];
                        const GamepadInput& input = config->m_Inputs[trigger.m_Input];
                        if (input.m_Index != (uint16_t)~0)
                        {
                            float v = ApplyGamepadModifiers(&binding->m_GamepadBinding->m_Packet, input);
                            Action* action = binding->m_Actions.Get(trigger.m_ActionId);
                            if (action != 0x0)
                            {
                                if (dmMath::Abs(action->m_Value) < dmMath::Abs(v))
                                    action->m_Value = v;
                            }
                        }
                    }
                    binding->m_GamepadBinding->m_PreviousPacket = binding->m_GamepadBinding->m_Packet;
                }
                else
                {
                    if (!binding->m_GamepadBinding->m_NoMapWarning)
                    {
                        dmLogWarning("No gamepad map registered for gamepad %d, not used.", binding->m_GamepadBinding->m_Index);
                        binding->m_GamepadBinding->m_NoMapWarning = 1;
                    }
                }
            }
        }
        UpdateContext context;
        context.m_DT = dt;
        context.m_Context = binding->m_Context;
        binding->m_Actions.Iterate<void>(UpdateAction, &context);
    }

    float GetValue(HBinding binding, dmhash_t action_id)
    {
        Action* action = binding->m_Actions.Get(action_id);
        if (action != 0x0)
            return action->m_Value;
        else
            return 0.0f;
    }

    bool Pressed(HBinding binding, dmhash_t action_id)
    {
        Action* action = binding->m_Actions.Get(action_id);
        if (action != 0x0)
            return action->m_Pressed;
        else
            return false;
    }

    bool Released(HBinding binding, dmhash_t action_id)
    {
        Action* action = binding->m_Actions.Get(action_id);
        if (action != 0x0)
            return action->m_Released;
        else
            return false;
    }

    bool Repeated(HBinding binding, dmhash_t action_id)
    {
        Action* action = binding->m_Actions.Get(action_id);
        if (action != 0x0)
            return action->m_Repeated;
        else
            return false;
    }

    struct CallbackData
    {
        ActionCallback m_Callback;
        void* m_UserData;
    };

    void ForEachActiveCallback(CallbackData* data, const dmhash_t* key, Action* action)
    {
        if (action->m_Value != 0.0f || action->m_Pressed || action->m_Released)
            data->m_Callback(*key, action, data->m_UserData);
    }

    void ForEachActive(HBinding binding, ActionCallback callback, void* user_data)
    {
        CallbackData data;
        data.m_Callback = callback;
        data.m_UserData = user_data;
        binding->m_Actions.Iterate<CallbackData>(ForEachActiveCallback, &data);
    }

    float ApplyGamepadModifiers(dmHID::GamepadPacket* packet, const GamepadInput& input)
    {
        float v = 0.0f;
        switch (input.m_Type)
        {
        case dmInputDDF::GAMEPAD_TYPE_AXIS:
            v = packet->m_Axis[input.m_Index];
            if (input.m_Negate)
                v = -v;
            if (input.m_Scale)
                v = (v + 1.0f) * 0.5f;
            if (input.m_Clamp)
                v = dmMath::Clamp(v, 0.0f, 1.0f);
            break;
        case dmInputDDF::GAMEPAD_TYPE_BUTTON:
            v = dmHID::GetGamepadButton(packet, input.m_Index) ? 1.0f : 0.0f;
            break;
        }
        return v;
    }

    void InitKeyMap()
    {
        KEY_MAP[dmInputDDF::KEY_SPACE] = dmHID::KEY_SPACE;
        KEY_MAP[dmInputDDF::KEY_EXCLAIM] = dmHID::KEY_EXCLAIM;
        KEY_MAP[dmInputDDF::KEY_QUOTEDBL] = dmHID::KEY_QUOTEDBL;
        KEY_MAP[dmInputDDF::KEY_HASH] = dmHID::KEY_HASH;
        KEY_MAP[dmInputDDF::KEY_DOLLAR] = dmHID::KEY_DOLLAR;
        KEY_MAP[dmInputDDF::KEY_AMPERSAND] = dmHID::KEY_AMPERSAND;
        KEY_MAP[dmInputDDF::KEY_QUOTE] = dmHID::KEY_QUOTE;
        KEY_MAP[dmInputDDF::KEY_LPAREN] = dmHID::KEY_LPAREN;
        KEY_MAP[dmInputDDF::KEY_RPAREN] = dmHID::KEY_RPAREN;
        KEY_MAP[dmInputDDF::KEY_ASTERISK] = dmHID::KEY_ASTERISK;
        KEY_MAP[dmInputDDF::KEY_PLUS] = dmHID::KEY_PLUS;
        KEY_MAP[dmInputDDF::KEY_COMMA] = dmHID::KEY_COMMA;
        KEY_MAP[dmInputDDF::KEY_MINUS] = dmHID::KEY_MINUS;
        KEY_MAP[dmInputDDF::KEY_PERIOD] = dmHID::KEY_PERIOD;
        KEY_MAP[dmInputDDF::KEY_SLASH] = dmHID::KEY_SLASH;
        KEY_MAP[dmInputDDF::KEY_0] = dmHID::KEY_0;
        KEY_MAP[dmInputDDF::KEY_1] = dmHID::KEY_1;
        KEY_MAP[dmInputDDF::KEY_2] = dmHID::KEY_2;
        KEY_MAP[dmInputDDF::KEY_3] = dmHID::KEY_3;
        KEY_MAP[dmInputDDF::KEY_4] = dmHID::KEY_4;
        KEY_MAP[dmInputDDF::KEY_5] = dmHID::KEY_5;
        KEY_MAP[dmInputDDF::KEY_6] = dmHID::KEY_6;
        KEY_MAP[dmInputDDF::KEY_7] = dmHID::KEY_7;
        KEY_MAP[dmInputDDF::KEY_8] = dmHID::KEY_8;
        KEY_MAP[dmInputDDF::KEY_9] = dmHID::KEY_9;
        KEY_MAP[dmInputDDF::KEY_COLON] = dmHID::KEY_COLON;
        KEY_MAP[dmInputDDF::KEY_SEMICOLON] = dmHID::KEY_SEMICOLON;
        KEY_MAP[dmInputDDF::KEY_LESS] = dmHID::KEY_LESS;
        KEY_MAP[dmInputDDF::KEY_EQUALS] = dmHID::KEY_EQUALS;
        KEY_MAP[dmInputDDF::KEY_GREATER] = dmHID::KEY_GREATER;
        KEY_MAP[dmInputDDF::KEY_QUESTION] = dmHID::KEY_QUESTION;
        KEY_MAP[dmInputDDF::KEY_AT] = dmHID::KEY_AT;
        KEY_MAP[dmInputDDF::KEY_A] = dmHID::KEY_A;
        KEY_MAP[dmInputDDF::KEY_B] = dmHID::KEY_B;
        KEY_MAP[dmInputDDF::KEY_C] = dmHID::KEY_C;
        KEY_MAP[dmInputDDF::KEY_D] = dmHID::KEY_D;
        KEY_MAP[dmInputDDF::KEY_E] = dmHID::KEY_E;
        KEY_MAP[dmInputDDF::KEY_F] = dmHID::KEY_F;
        KEY_MAP[dmInputDDF::KEY_G] = dmHID::KEY_G;
        KEY_MAP[dmInputDDF::KEY_H] = dmHID::KEY_H;
        KEY_MAP[dmInputDDF::KEY_I] = dmHID::KEY_I;
        KEY_MAP[dmInputDDF::KEY_J] = dmHID::KEY_J;
        KEY_MAP[dmInputDDF::KEY_K] = dmHID::KEY_K;
        KEY_MAP[dmInputDDF::KEY_L] = dmHID::KEY_L;
        KEY_MAP[dmInputDDF::KEY_M] = dmHID::KEY_M;
        KEY_MAP[dmInputDDF::KEY_N] = dmHID::KEY_N;
        KEY_MAP[dmInputDDF::KEY_O] = dmHID::KEY_O;
        KEY_MAP[dmInputDDF::KEY_P] = dmHID::KEY_P;
        KEY_MAP[dmInputDDF::KEY_Q] = dmHID::KEY_Q;
        KEY_MAP[dmInputDDF::KEY_R] = dmHID::KEY_R;
        KEY_MAP[dmInputDDF::KEY_S] = dmHID::KEY_S;
        KEY_MAP[dmInputDDF::KEY_T] = dmHID::KEY_T;
        KEY_MAP[dmInputDDF::KEY_U] = dmHID::KEY_U;
        KEY_MAP[dmInputDDF::KEY_V] = dmHID::KEY_V;
        KEY_MAP[dmInputDDF::KEY_W] = dmHID::KEY_W;
        KEY_MAP[dmInputDDF::KEY_X] = dmHID::KEY_X;
        KEY_MAP[dmInputDDF::KEY_Y] = dmHID::KEY_Y;
        KEY_MAP[dmInputDDF::KEY_Z] = dmHID::KEY_Z;
        KEY_MAP[dmInputDDF::KEY_LBRACKET] = dmHID::KEY_LBRACKET;
        KEY_MAP[dmInputDDF::KEY_BACKSLASH] = dmHID::KEY_BACKSLASH;
        KEY_MAP[dmInputDDF::KEY_RBRACKET] = dmHID::KEY_RBRACKET;
        KEY_MAP[dmInputDDF::KEY_CARET] = dmHID::KEY_CARET;
        KEY_MAP[dmInputDDF::KEY_UNDERSCORE] = dmHID::KEY_UNDERSCORE;
        KEY_MAP[dmInputDDF::KEY_BACKQUOTE] = dmHID::KEY_BACKQUOTE;
        KEY_MAP[dmInputDDF::KEY_LBRACE] = dmHID::KEY_LBRACE;
        KEY_MAP[dmInputDDF::KEY_PIPE] = dmHID::KEY_PIPE;
        KEY_MAP[dmInputDDF::KEY_RBRACE] = dmHID::KEY_RBRACE;
        KEY_MAP[dmInputDDF::KEY_TILDE] = dmHID::KEY_TILDE;
        KEY_MAP[dmInputDDF::KEY_ESC] = dmHID::KEY_ESC;
        KEY_MAP[dmInputDDF::KEY_F1] = dmHID::KEY_F1;
        KEY_MAP[dmInputDDF::KEY_F2] = dmHID::KEY_F2;
        KEY_MAP[dmInputDDF::KEY_F3] = dmHID::KEY_F3;
        KEY_MAP[dmInputDDF::KEY_F4] = dmHID::KEY_F4;
        KEY_MAP[dmInputDDF::KEY_F5] = dmHID::KEY_F5;
        KEY_MAP[dmInputDDF::KEY_F6] = dmHID::KEY_F6;
        KEY_MAP[dmInputDDF::KEY_F7] = dmHID::KEY_F7;
        KEY_MAP[dmInputDDF::KEY_F8] = dmHID::KEY_F8;
        KEY_MAP[dmInputDDF::KEY_F9] = dmHID::KEY_F9;
        KEY_MAP[dmInputDDF::KEY_F10] = dmHID::KEY_F10;
        KEY_MAP[dmInputDDF::KEY_F11] = dmHID::KEY_F11;
        KEY_MAP[dmInputDDF::KEY_F12] = dmHID::KEY_F12;
        KEY_MAP[dmInputDDF::KEY_UP] = dmHID::KEY_UP;
        KEY_MAP[dmInputDDF::KEY_DOWN] = dmHID::KEY_DOWN;
        KEY_MAP[dmInputDDF::KEY_LEFT] = dmHID::KEY_LEFT;
        KEY_MAP[dmInputDDF::KEY_RIGHT] = dmHID::KEY_RIGHT;
        KEY_MAP[dmInputDDF::KEY_LSHIFT] = dmHID::KEY_LSHIFT;
        KEY_MAP[dmInputDDF::KEY_RSHIFT] = dmHID::KEY_RSHIFT;
        KEY_MAP[dmInputDDF::KEY_LCTRL] = dmHID::KEY_LCTRL;
        KEY_MAP[dmInputDDF::KEY_RCTRL] = dmHID::KEY_RCTRL;
        KEY_MAP[dmInputDDF::KEY_LALT] = dmHID::KEY_LALT;
        KEY_MAP[dmInputDDF::KEY_RALT] = dmHID::KEY_RALT;
        KEY_MAP[dmInputDDF::KEY_TAB] = dmHID::KEY_TAB;
        KEY_MAP[dmInputDDF::KEY_ENTER] = dmHID::KEY_ENTER;
        KEY_MAP[dmInputDDF::KEY_BACKSPACE] = dmHID::KEY_BACKSPACE;
        KEY_MAP[dmInputDDF::KEY_INSERT] = dmHID::KEY_INSERT;
        KEY_MAP[dmInputDDF::KEY_DEL] = dmHID::KEY_DEL;
        KEY_MAP[dmInputDDF::KEY_PAGEUP] = dmHID::KEY_PAGEUP;
        KEY_MAP[dmInputDDF::KEY_PAGEDOWN] = dmHID::KEY_PAGEDOWN;
        KEY_MAP[dmInputDDF::KEY_HOME] = dmHID::KEY_HOME;
        KEY_MAP[dmInputDDF::KEY_END] = dmHID::KEY_END;
        KEY_MAP[dmInputDDF::KEY_KP_0] = dmHID::KEY_KP_0;
        KEY_MAP[dmInputDDF::KEY_KP_1] = dmHID::KEY_KP_1;
        KEY_MAP[dmInputDDF::KEY_KP_2] = dmHID::KEY_KP_2;
        KEY_MAP[dmInputDDF::KEY_KP_3] = dmHID::KEY_KP_3;
        KEY_MAP[dmInputDDF::KEY_KP_4] = dmHID::KEY_KP_4;
        KEY_MAP[dmInputDDF::KEY_KP_5] = dmHID::KEY_KP_5;
        KEY_MAP[dmInputDDF::KEY_KP_6] = dmHID::KEY_KP_6;
        KEY_MAP[dmInputDDF::KEY_KP_7] = dmHID::KEY_KP_7;
        KEY_MAP[dmInputDDF::KEY_KP_8] = dmHID::KEY_KP_8;
        KEY_MAP[dmInputDDF::KEY_KP_9] = dmHID::KEY_KP_9;
        KEY_MAP[dmInputDDF::KEY_KP_DIVIDE] = dmHID::KEY_KP_DIVIDE;
        KEY_MAP[dmInputDDF::KEY_KP_MULTIPLY] = dmHID::KEY_KP_MULTIPLY;
        KEY_MAP[dmInputDDF::KEY_KP_SUBTRACT] = dmHID::KEY_KP_SUBTRACT;
        KEY_MAP[dmInputDDF::KEY_KP_ADD] = dmHID::KEY_KP_ADD;
        KEY_MAP[dmInputDDF::KEY_KP_DECIMAL] = dmHID::KEY_KP_DECIMAL;
        KEY_MAP[dmInputDDF::KEY_KP_EQUAL] = dmHID::KEY_KP_EQUAL;
        KEY_MAP[dmInputDDF::KEY_KP_ENTER] = dmHID::KEY_KP_ENTER;
    }

    void InitMouseButtonMap()
    {
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_LEFT] = dmHID::MOUSE_BUTTON_LEFT;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_MIDDLE] = dmHID::MOUSE_BUTTON_MIDDLE;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_RIGHT] = dmHID::MOUSE_BUTTON_RIGHT;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_1] = dmHID::MOUSE_BUTTON_1;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_2] = dmHID::MOUSE_BUTTON_2;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_3] = dmHID::MOUSE_BUTTON_3;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_4] = dmHID::MOUSE_BUTTON_4;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_5] = dmHID::MOUSE_BUTTON_5;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_6] = dmHID::MOUSE_BUTTON_6;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_7] = dmHID::MOUSE_BUTTON_7;
        MOUSE_BUTTON_MAP[dmInputDDF::MOUSE_BUTTON_8] = dmHID::MOUSE_BUTTON_8;
    }
}
