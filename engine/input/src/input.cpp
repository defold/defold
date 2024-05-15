// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <string.h>
#include <stdio.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/platform.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>

#include "input.h"
#include "input_private.h"

namespace dmInput
{
    const uint32_t UNKNOWN_GAMEPAD_CONFIG_ID = dmHashString32("UNKNOWN_GAMEPAD_CONFIG_ID");
    static const uint16_t INVALID_INDEX = 0xFFFF;


    dmHID::Key KEY_MAP[dmInputDDF::MAX_KEY_COUNT];
    dmHID::MouseButton MOUSE_BUTTON_MAP[dmInputDDF::MAX_KEY_COUNT];

    void InitKeyMap();
    void InitMouseButtonMap();
    bool g_Init = false;

    HContext NewContext(const NewContextParams& params)
    {
        if (!g_Init)
        {
            InitKeyMap();
            InitMouseButtonMap();
            g_Init = true;
        }
        Context* context = new Context();
        context->m_GamepadIndices.SetCapacity(16);
        context->m_UnmappedGamepads.SetCapacity(7, 16);
        context->m_HidContext = params.m_HidContext;
        context->m_RepeatDelay = params.m_RepeatDelay;
        context->m_RepeatInterval = params.m_RepeatInterval;
        context->m_PressedThreshold = 0.9f;
        return context;
    }

    void DeleteContext(HContext context)
    {
        delete context;
    }

    static void MapGamePad(Binding* binding, uint32_t gamepad_index, bool connected);

    static void MapGamePadCbk(void* binding, const uint32_t* gamepad_index, bool* connected)
    {
        MapGamePad((Binding*)binding, *gamepad_index, *connected);
    }

    void Update(HContext context)
    {
        if (context->m_UnmappedGamepads.Size() && context->m_Binding)
        {
            context->m_UnmappedGamepads.Iterate<void>(MapGamePadCbk, context->m_Binding);
            context->m_UnmappedGamepads.Clear();
        }
    }

    void SetRepeat(HContext context, float delay, float interval)
    {
        context->m_RepeatDelay = delay;
        context->m_RepeatInterval = interval;
    }

    HBinding NewBinding(HContext context)
    {
        Binding* binding = new Binding();
        binding->m_Context = context;
        binding->m_Actions.Clear();
        binding->m_Actions.SetCapacity(64, 256);
        binding->m_GamepadBindings.SetCapacity(dmHID::MAX_GAMEPAD_COUNT);

        binding->m_TextBinding = 0;
        binding->m_AccelerationBinding = 0;
        binding->m_TouchDeviceBinding = 0;
        binding->m_MouseBinding = 0;
        binding->m_KeyboardBinding = 0;
        binding->m_DDFGamepadTriggersCount = 0;
        binding->m_DDFGamepadTriggersData = 0;
        binding->m_DDFGamepadTriggersCount = 0;

        context->m_Binding = binding;
        return binding;
    }

    static void ResetGamepadBindings(HBinding binding, GamepadBinding* gamepad_binding, uint32_t gamepad_index)
    {
        Action action;
        memset(&action, 0, sizeof(Action));
        action.m_IsGamepad = 1;
        action.m_GamepadUnknown = gamepad_binding->m_Unknown;

        gamepad_binding->m_Index = gamepad_index;

        gamepad_binding->m_Triggers.SetCapacity(binding->m_DDFGamepadTriggersCount);
        gamepad_binding->m_Triggers.SetSize(0);
        gamepad_binding->m_Actions.Clear();
        gamepad_binding->m_Actions.SetCapacity(64, 256);

        action.m_GamepadIndex = gamepad_binding->m_Index;
        action.m_UserID       = 0;

        for (uint32_t i = 0; i < binding->m_DDFGamepadTriggersCount; ++i)
        {
            const dmInputDDF::GamepadTrigger& ddf_trigger = binding->m_DDFGamepadTriggersData[i];
            dmInput::GamepadTrigger trigger;
            trigger.m_ActionId = dmHashString64(ddf_trigger.m_Action);
            trigger.m_Input = ddf_trigger.m_Input;
            gamepad_binding->m_Triggers.Push(trigger);
            gamepad_binding->m_Actions.Put(trigger.m_ActionId, action);
        }
    }

    static GamepadConfig* GetGamepadConfigFromDeviceName(HBinding binding, const uint32_t device_name_hash)
    {
        GamepadConfig* config = binding->m_Context->m_GamepadMaps.Get(device_name_hash);
        if (config == 0x0)
        {
            config = binding->m_Context->m_GamepadMaps.Get(UNKNOWN_GAMEPAD_CONFIG_ID);
        }
        return config;
    }

    static GamepadBinding* NewGamepadBinding(HBinding binding, uint32_t gamepad_index)
    {
        dmHID::HGamepad gamepad = dmHID::GetGamepad(binding->m_Context->m_HidContext, gamepad_index);
        char device_name[128];
        dmHID::GetGamepadDeviceName(binding->m_Context->m_HidContext, gamepad, device_name, sizeof(device_name));
        if (strlen(device_name) == 0)
        {
            /*
             * NOTE: We used to log a warning here but the warning is removed for the following reasons:
             *  - The input-binding file covers several platforms and certain platforms
             *    doesn't have support for e.g. pads. But more importantly, sometimes you might have
             *    a device connected but sometimes not. It should be up to the user and we shouldn't
             *    spam out warnings in such cases. In other words. It's impossible to tell whether the
             *    warning is appropriate or not.
             *  - We should also have support dynamic pad-connections. Whether a pad is connected
             *    or not should be up to the game-ui.
             */
            return 0x0;
        } else {
            GamepadConfig* config = GetGamepadConfigFromDeviceName(binding, dmHashString32(device_name));
            if (config == 0x0)
            {
                dmLogWarning("No gamepad map found for gamepad %d (%s). Ignored.", gamepad_index, device_name);
                return 0x0;
            }
            GamepadBinding* gamepad_binding = new GamepadBinding();
            memset(gamepad_binding, 0, sizeof(*gamepad_binding));
            gamepad_binding->m_Gamepad = gamepad;
            if (config->m_DeviceId == UNKNOWN_GAMEPAD_CONFIG_ID)
            {
                dmLogWarning("No gamepad map found for gamepad %d (%s). The raw gamepad map will be used.", gamepad_index, device_name);
                gamepad_binding->m_Unknown = 1;
            }

            ResetGamepadBindings(binding, gamepad_binding, gamepad_index);

            return gamepad_binding;
        }
    }

    static void SetupGamepadBindings(HBinding binding)
    {
        for (uint32_t i = 0; i < binding->m_GamepadBindings.Size(); ++i)
        {
            delete binding->m_GamepadBindings[i];
        }
        binding->m_GamepadBindings.SetSize(0);
        for (uint32_t gamepad_index = 0; gamepad_index < dmHID::MAX_GAMEPAD_COUNT; ++gamepad_index)
        {
            GamepadBinding* gamepad_binding = NewGamepadBinding(binding, gamepad_index);
            if (gamepad_binding)
            {
                binding->m_GamepadBindings.Push(gamepad_binding);
            }
        }
    }

    static dmInputDDF::GamepadTrigger* CopyGamepadTrigger(const dmInputDDF::GamepadTrigger* orig, uint32_t count)
    {
        dmInputDDF::GamepadTrigger* copy = new dmInputDDF::GamepadTrigger[count];

        for (uint32_t i = 0; i < count; ++i)
        {
            copy[i].m_Input = orig[i].m_Input;
            size_t str_len = strlen(orig[i].m_Action) + 1;
            copy[i].m_Action = new char[str_len];
            dmStrlCpy((char*)copy[i].m_Action, orig[i].m_Action, str_len);
            ((char*)copy[i].m_Action)[str_len-1] = '\0';
        }

        return copy;
    }

    static void DeleteGamepadTrigger(dmInputDDF::GamepadTrigger* triggers, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            delete [] triggers[i].m_Action;
        }

        delete [] triggers;
    }

    void SetBinding(HBinding binding, dmInputDDF::InputBinding* ddf)
    {
        binding->m_Actions.Clear();
        Action action;
        memset(&action, 0, sizeof(Action));
        // add null action for mouse movement
        binding->m_Actions.Put(0, action);
        if (ddf->m_KeyTrigger.m_Count > 0)
        {
            if (binding->m_KeyboardBinding == 0x0)
            {
                binding->m_KeyboardBinding = new KeyboardBinding();
                memset(binding->m_KeyboardBinding, 0, sizeof(*binding->m_KeyboardBinding));

                binding->m_KeyboardBinding->m_Keyboard = dmHID::GetKeyboard(binding->m_Context->m_HidContext, 0);
            }
            else
            {
                binding->m_KeyboardBinding->m_Triggers.SetSize(0);
            }
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
        else if (binding->m_KeyboardBinding != 0x0)
        {
            delete binding->m_KeyboardBinding;
            binding->m_KeyboardBinding = 0x0;
        }
        if (ddf->m_MouseTrigger.m_Count > 0)
        {
            if (binding->m_MouseBinding == 0x0)
            {
                binding->m_MouseBinding = new MouseBinding();
                memset(binding->m_MouseBinding, 0, sizeof(*binding->m_MouseBinding));

                binding->m_MouseBinding->m_Mouse = dmHID::GetMouse(binding->m_Context->m_HidContext, 0);
            }
            else
            {
                binding->m_MouseBinding->m_Triggers.SetSize(0);
            }
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
            // Mouse move action
            binding->m_Actions.Put(0, action);
        }
        else if (binding->m_MouseBinding != 0x0)
        {
            delete binding->m_MouseBinding;
            binding->m_MouseBinding = 0x0;
        }

        if (binding->m_DDFGamepadTriggersData) {
            DeleteGamepadTrigger(binding->m_DDFGamepadTriggersData, binding->m_DDFGamepadTriggersCount);
        }
        binding->m_DDFGamepadTriggersData = NULL;
        binding->m_DDFGamepadTriggersCount = ddf->m_GamepadTrigger.m_Count;
        if (ddf->m_GamepadTrigger.m_Count > 0)
        {
            binding->m_DDFGamepadTriggersData = CopyGamepadTrigger(ddf->m_GamepadTrigger.m_Data, binding->m_DDFGamepadTriggersCount);
            SetupGamepadBindings(binding);
        }

        uint32_t touch_count = ddf->m_TouchTrigger.m_Count;
        if (touch_count > 0)
        {
            if (binding->m_TouchDeviceBinding == 0x0)
            {
                binding->m_TouchDeviceBinding = new TouchDeviceBinding();
                memset(binding->m_TouchDeviceBinding, 0, sizeof(*binding->m_TouchDeviceBinding));

                binding->m_TouchDeviceBinding->m_TouchDevice = dmHID::GetTouchDevice(binding->m_Context->m_HidContext, 0);
            }
            else
            {
                binding->m_TouchDeviceBinding->m_Triggers.SetSize(0);
            }
            binding->m_TouchDeviceBinding->m_Triggers.SetCapacity(touch_count);
            for (uint32_t i = 0; i < touch_count; ++i)
            {
                const dmInputDDF::TouchTrigger& ddf_trigger = ddf->m_TouchTrigger[i];
                dmInput::TouchTrigger trigger;
                trigger.m_ActionId = dmHashString64(ddf_trigger.m_Action);
                trigger.m_Input = ddf_trigger.m_Input;
                binding->m_TouchDeviceBinding->m_Triggers.Push(trigger);
                binding->m_Actions.Put(trigger.m_ActionId, action);
            }
            // Mouse move action
            binding->m_Actions.Put(0, action);
        }
        else if (binding->m_TouchDeviceBinding != 0x0)
        {
            delete binding->m_TouchDeviceBinding;
            binding->m_TouchDeviceBinding = 0x0;
        }

        uint32_t text_count = ddf->m_TextTrigger.m_Count;
        if (text_count > 0)
        {
            if (binding->m_TextBinding == 0x0)
            {
                binding->m_TextBinding = new TextBinding();
                memset(binding->m_TextBinding, 0, sizeof(*binding->m_TextBinding));
            }
            else
            {
                binding->m_TextBinding->m_Triggers.SetSize(0);
            }
            binding->m_TextBinding->m_Triggers.SetCapacity(text_count);
            for (uint32_t i = 0; i < text_count; ++i)
            {
                const dmInputDDF::TextTrigger& ddf_trigger = ddf->m_TextTrigger[i];
                dmInput::TextTrigger trigger;
                trigger.m_ActionId = dmHashString64(ddf_trigger.m_Action);
                trigger.m_Input = ddf_trigger.m_Input;
                binding->m_TextBinding->m_Triggers.Push(trigger);
                binding->m_Actions.Put(trigger.m_ActionId, action);
            }
            binding->m_Actions.Put(0, action);
        }
        else if (binding->m_TextBinding != 0x0)
        {
            delete binding->m_TextBinding;
            binding->m_TextBinding = 0x0;
        }

        if (binding->m_AccelerationBinding == 0x0)
        {
            binding->m_AccelerationBinding = new AccelerationBinding();
            memset(binding->m_AccelerationBinding, 0, sizeof(*binding->m_AccelerationBinding));
        }
    }

    void DeleteBinding(HBinding binding)
    {
        binding->m_Context->m_Binding = 0;

        if (binding->m_KeyboardBinding != 0x0)
            delete binding->m_KeyboardBinding;
        if (binding->m_MouseBinding != 0x0)
            delete binding->m_MouseBinding;
        for (uint32_t i = 0; i < binding->m_GamepadBindings.Size(); ++i)
        {
            delete binding->m_GamepadBindings[i];
        }
        if (binding->m_TouchDeviceBinding != 0x0)
            delete binding->m_TouchDeviceBinding;
        if (binding->m_AccelerationBinding != 0x0)
            delete binding->m_AccelerationBinding;
        if (binding->m_TextBinding != 0x0)
            delete binding->m_TextBinding;
        if (binding->m_DDFGamepadTriggersData != 0x0) {
            DeleteGamepadTrigger(binding->m_DDFGamepadTriggersData, binding->m_DDFGamepadTriggersCount);
        }
        delete binding;
    }

    static inline bool SupportsPlatform(const char* platform)
    {
        if (strcmp(DM_PLATFORM, platform) == 0)
            return true;
#if defined(__APPLE__)
        if (strcmp("osx", platform) == 0)
            return true;
#endif
        return false;
    }

    void RegisterGamepads(HContext context, const dmInputDDF::GamepadMaps* ddf)
    {
        int count = 0;
        for (uint32_t i = 0; i < ddf->m_Driver.m_Count; ++i)
        {
            const dmInputDDF::GamepadMap& gamepad_map = ddf->m_Driver[i];
            if (SupportsPlatform(gamepad_map.m_Platform))
            {
                count++;
            }
        }

        context->m_GamepadMaps.SetCapacity(dmMath::Max(1, (count + 1) / 3), count + 1);

        // Add a gamepad config that will be used when an unidentified gamepad
        // is connected. This will allow developers to at least read raw button,
        // axis and hat data and do their own mappings.
        GamepadConfig unknownGamepadConfig;
        unknownGamepadConfig.m_DeadZone = 0.0;
        unknownGamepadConfig.m_DeviceId = UNKNOWN_GAMEPAD_CONFIG_ID;
        memset(unknownGamepadConfig.m_Inputs, 0, sizeof(unknownGamepadConfig.m_Inputs));
        for (uint32_t j = 0; j < (sizeof(unknownGamepadConfig.m_Inputs) / sizeof(struct GamepadInput)); ++j)
        {
            unknownGamepadConfig.m_Inputs[j].m_Index = INVALID_INDEX;
        }
        context->m_GamepadMaps.Put(UNKNOWN_GAMEPAD_CONFIG_ID, unknownGamepadConfig);

        if (count == 0)
        {
            return;
        }

        for (uint32_t i = 0; i < ddf->m_Driver.m_Count; ++i)
        {
            const dmInputDDF::GamepadMap& gamepad_map = ddf->m_Driver[i];
            if (SupportsPlatform(gamepad_map.m_Platform))
            {
                uint32_t device_id = dmHashString32(gamepad_map.m_Device);
                if (context->m_GamepadMaps.Get(device_id) == 0x0)
                {
                    GamepadConfig config;
                    config.m_DeadZone = gamepad_map.m_DeadZone;
                    config.m_DeviceId = device_id;
                    memset(config.m_Inputs, 0, sizeof(config.m_Inputs));
                    for (uint32_t j = 0; j < (sizeof(config.m_Inputs) / sizeof(struct GamepadInput)); ++j)
                    {
                        config.m_Inputs[j].m_Index = INVALID_INDEX;
                    }

                    for (uint32_t j = 0; j < gamepad_map.m_Map.m_Count; ++j)
                    {
                        const dmInputDDF::GamepadMapEntry& entry = gamepad_map.m_Map[j];
                        GamepadInput& input = config.m_Inputs[entry.m_Input];
                        input.m_Index = (uint16_t)entry.m_Index;
                        input.m_Type = entry.m_Type;
                        if (entry.m_Type == dmInputDDF::GAMEPAD_TYPE_HAT)
                        {
                            input.m_HatMask = entry.m_HatMask;
                        }
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
        action->m_PositionSet = 0;
        action->m_AccelerationSet = 0;
        action->m_TouchCount = 0;
        action->m_TextCount = 0;
        action->m_HasText = 0;
        action->m_GamepadDisconnected = 0;
        action->m_GamepadConnected = 0;
        action->m_HasGamepadPacket = 0;
    }

    struct UpdateActionContext
    {
        UpdateActionContext()
        {
            memset(this, 0, sizeof(UpdateActionContext));
        }

        float m_DT;
        Context* m_Context;
        int32_t m_X;
        int32_t m_Y;
        int32_t m_DX;
        int32_t m_DY;
        float m_AccX;
        float m_AccY;
        float m_AccZ;
        uint32_t m_PositionSet : 1;
        uint32_t m_AccelerationSet : 1;
    };

    void UpdateAction(void* context, const dmhash_t* id, Action* action)
    {
        UpdateActionContext* update_context = (UpdateActionContext*)context;

        float pressed_threshold = update_context->m_Context->m_PressedThreshold;
        action->m_Pressed = (action->m_PrevValue < pressed_threshold && action->m_Value >= pressed_threshold) ? 1 : 0;
        action->m_Released = (action->m_PrevValue >= pressed_threshold && action->m_Value < pressed_threshold) ? 1 : 0;
        action->m_Repeated = false;
        if (action->m_Value > 0.0f)
        {
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
        if (action->m_PositionSet == 0)
        {
            action->m_X = update_context->m_X;
            action->m_Y = update_context->m_Y;
            action->m_DX = update_context->m_DX;
            action->m_DY = update_context->m_DY;
            action->m_PositionSet = update_context->m_PositionSet;
        }
        if (action->m_AccelerationSet == 0)
        {
            action->m_AccX = update_context->m_AccX;
            action->m_AccY = update_context->m_AccY;
            action->m_AccZ = update_context->m_AccZ;
            action->m_AccelerationSet = update_context->m_AccelerationSet;
        }
    }

    void UpdateBinding(HBinding binding, float dt)
    {
        DM_PROFILE(__FUNCTION__);
        binding->m_Actions.Iterate<void>(ClearAction, 0x0);

        dmHID::HContext hid_context = binding->m_Context->m_HidContext;
        UpdateActionContext context;
        if (binding->m_KeyboardBinding != 0x0)
        {
            KeyboardBinding* keyboard_binding = binding->m_KeyboardBinding;
            dmHID::KeyboardPacket* packet = &keyboard_binding->m_Packet;
            dmHID::KeyboardPacket* prev_packet = &keyboard_binding->m_PreviousPacket;
            if (dmHID::GetKeyboardPacket(keyboard_binding->m_Keyboard, packet))
            {
                const dmArray<KeyTrigger>& triggers = keyboard_binding->m_Triggers;
                for (uint32_t i = 0; i < triggers.Size(); ++i)
                {
                    const KeyTrigger& trigger = triggers[i];
                    float v = dmHID::GetKey(packet, KEY_MAP[trigger.m_Input]) ? 1.0f : 0.0f;
                    Action* action = binding->m_Actions.Get(trigger.m_ActionId);
                    if (action != 0x0)
                    {
                        if (dmMath::Abs(action->m_Value) < v)
                            action->m_Value = v;
                    }
                }
                *prev_packet = *packet;
            }
        }
        if (binding->m_TextBinding != 0x0)
        {
            TextBinding* text_binding = binding->m_TextBinding;

            dmHID::TextPacket* text_packet = &text_binding->m_TextPacket;
            if (dmHID::GetTextPacket(hid_context, text_packet))
            {
                const dmArray<TextTrigger>& triggers = text_binding->m_Triggers;
                for (uint32_t i = 0; i < triggers.Size(); ++i)
                {
                    const TextTrigger& trigger = triggers[i];
                    if (trigger.m_Input == dmInputDDF::TEXT)
                    {
                        Action* action = binding->m_Actions.Get(trigger.m_ActionId);
                        if (action != 0x0)
                        {
                            for (uint32_t i = 0; i < text_packet->m_Size; ++i) {
                                action->m_Text[i] = text_packet->m_Text[i];
                            }
                            action->m_TextCount = text_packet->m_Size;
                            action->m_HasText = action->m_TextCount > 0;
                        }
                    }
                }
            }

            dmHID::MarkedTextPacket* marked_packet = &text_binding->m_MarkedTextPacket;
            if (dmHID::GetMarkedTextPacket(hid_context, marked_packet))
            {
                const dmArray<TextTrigger>& triggers = text_binding->m_Triggers;
                for (uint32_t i = 0; i < triggers.Size(); ++i)
                {
                    const TextTrigger& trigger = triggers[i];
                    if (trigger.m_Input == dmInputDDF::MARKED_TEXT)
                    {
                        Action* action = binding->m_Actions.Get(trigger.m_ActionId);
                        if (action != 0x0)
                        {
                            for (uint32_t i = 0; i < marked_packet->m_Size; ++i) {
                                action->m_Text[i] = marked_packet->m_Text[i];
                            }
                            action->m_TextCount = marked_packet->m_Size;
                            action->m_HasText = marked_packet->m_HasText || action->m_TextCount > 0;
                        }
                    }
                }
            }
        }
        if (binding->m_MouseBinding != 0x0)
        {
            MouseBinding* mouse_binding = binding->m_MouseBinding;
            dmHID::MousePacket* packet = &mouse_binding->m_Packet;
            dmHID::MousePacket* prev_packet = &mouse_binding->m_PreviousPacket;
            if (dmHID::GetMousePacket(mouse_binding->m_Mouse, packet))
            {
                context.m_X = packet->m_PositionX;
                context.m_Y = packet->m_PositionY;
                context.m_DX = packet->m_PositionX - prev_packet->m_PositionX;
                context.m_DY = packet->m_PositionY - prev_packet->m_PositionY;
                context.m_PositionSet = 1;
                const dmArray<MouseTrigger>& triggers = mouse_binding->m_Triggers;
                for (uint32_t i = 0; i < triggers.Size(); ++i)
                {
                    const MouseTrigger& trigger = triggers[i];
                    float v = 0.0f;
                    switch (trigger.m_Input)
                    {
                    case dmInputDDF::MOUSE_WHEEL_UP:
                        v = (float) (packet->m_Wheel - prev_packet->m_Wheel);
                        break;
                    case dmInputDDF::MOUSE_WHEEL_DOWN:
                        v = (float) -(packet->m_Wheel - prev_packet->m_Wheel);
                        break;
                    default:
                        v = dmHID::GetMouseButton(packet, MOUSE_BUTTON_MAP[trigger.m_Input]) ? 1.0f : 0.0f;
                        break;
                    }
                    v = dmMath::Clamp(v, 0.0f, 1.0f);
                    Action* action = binding->m_Actions.Get(trigger.m_ActionId);
                    if (action != 0x0)
                    {
                        if (dmMath::Abs(action->m_Value) < dmMath::Abs(v))
                        {
                            action->m_Value = v;
                        }
                    }
                }
                *prev_packet = *packet;
            }
        }

        if (binding->m_GamepadBindings.Size() > 0)
        {
            for (uint32_t i = 0; i < binding->m_GamepadBindings.Size(); ++i)
            {
                GamepadBinding* gamepad_binding = binding->m_GamepadBindings[i];
                if (gamepad_binding == 0x0) {
                    continue;
                }
                gamepad_binding->m_Actions.Iterate<void>(ClearAction, 0x0);

                dmHID::Gamepad* gamepad = gamepad_binding->m_Gamepad;
                bool connected = dmHID::IsGamepadConnected(gamepad);
                if (!gamepad_binding->m_Connected)
                {
                    if (connected)
                    {
                        char device_name[128];
                        dmHID::GetGamepadDeviceName(binding->m_Context->m_HidContext, gamepad, device_name, sizeof(device_name));
                        gamepad_binding->m_DeviceId = dmHashString32(device_name);
                        gamepad_binding->m_Connected = 1;
                        gamepad_binding->m_NoMapWarning = 0;
                    }
                }

                bool prev_connected = gamepad_binding->m_Connected;
                gamepad_binding->m_Connected = connected;

                // Need to check if it was previously connected to be able
                // to pass along a disconnect trigger.
                if (connected || prev_connected)
                {
                    dmHID::GamepadPacket* packet = &gamepad_binding->m_Packet;

                    dmHID::GamepadPacket* prev_packet = &gamepad_binding->m_PreviousPacket;
                    GamepadConfig* config = GetGamepadConfigFromDeviceName(binding, gamepad_binding->m_DeviceId);
                    if (config != 0x0)
                    {
                        dmHID::GetGamepadPacket(gamepad, packet);

                        // Apply dead zone
                        uint32_t lstick_vert_axis = config->m_Inputs[dmInputDDF::GAMEPAD_LSTICK_UP].m_Index;
                        uint32_t lstick_hori_axis = config->m_Inputs[dmInputDDF::GAMEPAD_LSTICK_LEFT].m_Index;
                        uint32_t rstick_vert_axis = config->m_Inputs[dmInputDDF::GAMEPAD_RSTICK_UP].m_Index;
                        uint32_t rstick_hori_axis = config->m_Inputs[dmInputDDF::GAMEPAD_RSTICK_LEFT].m_Index;

                        if (lstick_vert_axis != INVALID_INDEX && lstick_hori_axis != INVALID_INDEX)
                        {
                            assert(lstick_vert_axis < dmHID::MAX_GAMEPAD_AXIS_COUNT);
                            assert(lstick_hori_axis < dmHID::MAX_GAMEPAD_AXIS_COUNT);

                            float& lx = packet->m_Axis[lstick_hori_axis];
                            float& ly = packet->m_Axis[lstick_vert_axis];
                            if (lx * lx + ly * ly <= config->m_DeadZone * config->m_DeadZone)
                            {
                                lx = 0.0f;
                                ly = 0.0f;
                            }
                        }
                        if (rstick_vert_axis != INVALID_INDEX && rstick_hori_axis != INVALID_INDEX)
                        {
                            assert(rstick_vert_axis < dmHID::MAX_GAMEPAD_AXIS_COUNT);
                            assert(rstick_hori_axis < dmHID::MAX_GAMEPAD_AXIS_COUNT);

                            float& rx = packet->m_Axis[rstick_hori_axis];
                            float& ry = packet->m_Axis[rstick_vert_axis];
                            if (rx * rx + ry * ry <= config->m_DeadZone * config->m_DeadZone)
                            {
                                rx = 0.0f;
                                ry = 0.0f;
                            }
                        }

                        const dmArray<GamepadTrigger>& triggers = gamepad_binding->m_Triggers;
                        for (uint32_t i = 0; i < triggers.Size(); ++i)
                        {
                            const GamepadTrigger& trigger = triggers[i];
                            const GamepadInput& input = config->m_Inputs[trigger.m_Input];

                            if ((trigger.m_Input == dmInputDDF::GAMEPAD_CONNECTED && packet->m_GamepadConnected) ||
                                (trigger.m_Input == dmInputDDF::GAMEPAD_DISCONNECTED && packet->m_GamepadDisconnected))
                            {
                                Action* action = gamepad_binding->m_Actions.Get(trigger.m_ActionId);
                                if (action != 0x0)
                                {
                                    action->m_GamepadDisconnected = packet->m_GamepadDisconnected;
                                    action->m_GamepadConnected = packet->m_GamepadConnected;

                                    if (action->m_GamepadConnected)
                                    {
                                        char device_name[128];
                                        dmHID::GetGamepadDeviceName(binding->m_Context->m_HidContext, gamepad, device_name, sizeof(device_name));
                                        action->m_TextCount = dmStrlCpy(action->m_Text, device_name, sizeof(action->m_Text));

                                        action->m_UserID = 0;
                                        dmHID::GetGamepadUserId(binding->m_Context->m_HidContext, gamepad_binding->m_Gamepad, &action->m_UserID);
                                    }
                                }
                            }
                            else if (trigger.m_Input == dmInputDDF::GAMEPAD_RAW)
                            {
                                Action* action = gamepad_binding->m_Actions.Get(trigger.m_ActionId);
                                if (action != 0x0)
                                {
                                    action->m_GamepadPacket = gamepad_binding->m_Packet;
                                    action->m_HasGamepadPacket = 1;
                                }
                            }
                            else
                            {
                                if (input.m_Index != INVALID_INDEX)
                                {
                                    float v = ApplyGamepadModifiers(packet, input);
                                    Action* action = gamepad_binding->m_Actions.Get(trigger.m_ActionId);
                                    if (action != 0x0)
                                    {
                                        if (dmMath::Abs(action->m_Value) < dmMath::Abs(v)) {
                                            action->m_Value = v;
                                        }

                                        // We want to make sure we report going back to 0 again
                                        action->m_Dirty = 0;
                                        if (input.m_Type == dmInputDDF::GAMEPAD_TYPE_AXIS && action->m_PrevValue != action->m_Value)
                                            action->m_Dirty = 1;
                                    }
                                }
                            }
                        }
                        *prev_packet = *packet;
                    }
                    else
                    {
                        if (!gamepad_binding->m_NoMapWarning)
                        {
                            dmLogWarning("No gamepad map registered for gamepad %d, not used.", gamepad_binding->m_Index);
                            gamepad_binding->m_NoMapWarning = 1;
                        }
                    }
                }
            }
        }
        if (binding->m_TouchDeviceBinding != 0x0)
        {
            TouchDeviceBinding* touch_device_binding = binding->m_TouchDeviceBinding;
            dmHID::TouchDevicePacket* packet = &touch_device_binding->m_Packet;
            dmHID::TouchDevicePacket* prev_packet = &touch_device_binding->m_PreviousPacket;
            if (dmHID::GetTouchDevicePacket(touch_device_binding->m_TouchDevice, packet))
            {
                const dmArray<TouchTrigger>& triggers = touch_device_binding->m_Triggers;
                for (uint32_t i = 0; i < triggers.Size(); ++i)
                {
                    const TouchTrigger& trigger = triggers[i];
                    Action* action = binding->m_Actions.Get(trigger.m_ActionId);
                    if (action != 0x0)
                    {
                        // TODO: Given the packet and prev_packet we could re-map the trigger inputs such that the summed deltas
                        // was minimized, giving continuous strokes of input

                        int32_t tn = packet->m_TouchCount;
                        // NOTE: We assume dmHID::MAX_TOUCH_COUNT for both source and destination here
                        assert(tn <= (int32_t) (sizeof(action->m_Touch) / sizeof(action->m_Touch[0])));
                        action->m_Value = 0;
                        for (int j = 0; j < tn; ++j) {
                            action->m_Touch[j] = packet->m_Touches[j];
                            dmHID::Phase p = packet->m_Touches[j].m_Phase;
                            if (j == 0)
                            {
                                action->m_X = action->m_Touch[j].m_X;
                                action->m_Y = action->m_Touch[j].m_Y;
                                action->m_DX = action->m_Touch[j].m_DX;
                                action->m_DY = action->m_Touch[j].m_DY;
                                action->m_PositionSet = 1;
                            }
                            if (p == dmHID::PHASE_BEGAN || p == dmHID::PHASE_MOVED || p == dmHID::PHASE_STATIONARY)
                            {
                                action->m_Value = 1.0;
                            }
                        }
                        action->m_TouchCount = packet->m_TouchCount;
                    }
                }
                *prev_packet = *packet;
            }
        }
        if (binding->m_AccelerationBinding != 0x0)
        {
            context.m_AccelerationSet = 0;
            if (dmHID::IsAccelerometerConnected(hid_context))
            {
                AccelerationBinding* acceleration_binding = binding->m_AccelerationBinding;
                dmHID::AccelerationPacket* packet = &acceleration_binding->m_Packet;
                dmHID::AccelerationPacket* prev_packet = &acceleration_binding->m_PreviousPacket;
                dmHID::GetAccelerationPacket(hid_context, packet);
                context.m_AccX = packet->m_X;
                context.m_AccY = packet->m_Y;
                context.m_AccZ = packet->m_Z;
                context.m_AccelerationSet = 1;
                *prev_packet = *packet;
            }
        }
        context.m_DT = dt;
        context.m_Context = binding->m_Context;
        binding->m_Actions.Iterate<void>(UpdateAction, &context);
        if (binding->m_GamepadBindings.Size() > 0)
        {
            for (uint32_t i = 0; i < binding->m_GamepadBindings.Size(); ++i)
            {
                GamepadBinding* gamepad_binding = binding->m_GamepadBindings[i];
                if (gamepad_binding == 0x0) {
                    continue;
                }
                gamepad_binding->m_Actions.Iterate<void>(UpdateAction, &context);
            }
        }

    }

    const Action* GetAction(HBinding binding, dmhash_t action_id)
    {
        return binding->m_Actions.Get(action_id);
    }

    float GetValue(HGamepadBinding binding, dmhash_t action_id)
    {
        Action* action = binding->m_Actions.Get(action_id);
        if (action != 0x0)
            return action->m_Value;
        else
            return 0.0f;
    }

    bool Pressed(HGamepadBinding binding, dmhash_t action_id)
    {
        Action* action = binding->m_Actions.Get(action_id);
        if (action != 0x0)
            return action->m_Pressed;
        else
            return false;
    }

    bool Released(HGamepadBinding binding, dmhash_t action_id)
    {
        Action* action = binding->m_Actions.Get(action_id);
        if (action != 0x0)
            return action->m_Released;
        else
            return false;
    }

    bool Repeated(HGamepadBinding binding, dmhash_t action_id)
    {
        Action* action = binding->m_Actions.Get(action_id);
        if (action != 0x0)
            return action->m_Repeated;
        else
            return false;
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
        bool active = action->m_Value != 0.0f || action->m_Pressed || action->m_Released || action->m_TouchCount > 0;
        active = active || action->m_GamepadConnected || action->m_GamepadDisconnected;
        active = active || action->m_Dirty; // e.g. for analog stick action being released
        active = active || action->m_HasGamepadPacket; // Raw gamepad data
        active = active || action->m_HasText; // Text input
        active = active || (*key == 0 && (action->m_DX != 0 || action->m_DY != 0 || action->m_AccelerationSet)); // Mouse move action
        if (active)
        {
            data->m_Callback(*key, action, data->m_UserData);
        }
    }

    void ForEachActive(HBinding binding, ActionCallback callback, void* user_data)
    {
        CallbackData data;
        data.m_Callback = callback;
        data.m_UserData = user_data;
        binding->m_Actions.Iterate<CallbackData>(ForEachActiveCallback, &data);
        if (binding->m_GamepadBindings.Size() > 0)
        {
            for (uint32_t i = 0; i < binding->m_GamepadBindings.Size(); ++i)
            {
                GamepadBinding* gamepad_binding = binding->m_GamepadBindings[i];
                if (gamepad_binding == 0x0) {
                    continue;
                }
                gamepad_binding->m_Actions.Iterate<CallbackData>(ForEachActiveCallback, &data);
            }
        }
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
        case dmInputDDF::GAMEPAD_TYPE_HAT:
            {
                uint8_t t = 0;
                bool s = dmHID::GetGamepadHat(packet, input.m_Index, &t);
                if (s && (t & input.m_HatMask)) {
                    v = 1.0f;
                }
            }
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
        KEY_MAP[dmInputDDF::KEY_KP_NUM_LOCK] = dmHID::KEY_KP_NUM_LOCK;
        KEY_MAP[dmInputDDF::KEY_CAPS_LOCK] = dmHID::KEY_CAPS_LOCK;
        KEY_MAP[dmInputDDF::KEY_SCROLL_LOCK] = dmHID::KEY_SCROLL_LOCK;
        KEY_MAP[dmInputDDF::KEY_PAUSE] = dmHID::KEY_PAUSE;
        KEY_MAP[dmInputDDF::KEY_LSUPER] = dmHID::KEY_LSUPER;
        KEY_MAP[dmInputDDF::KEY_RSUPER] = dmHID::KEY_RSUPER;
        KEY_MAP[dmInputDDF::KEY_MENU] = dmHID::KEY_MENU;
        KEY_MAP[dmInputDDF::KEY_BACK] = dmHID::KEY_BACK;
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

    static void MapGamePad(Binding* binding, uint32_t gamepad_index, bool connected)
    {
        if (connected)
        {
            GamepadBinding* gamepad_binding = 0x0;

            // Try to find if this gamepad index was connected before.
            for (uint32_t i = 0; i < binding->m_GamepadBindings.Size(); ++i)
            {
                GamepadBinding* t = binding->m_GamepadBindings[i];
                if (t->m_Index == gamepad_index) {
                    gamepad_binding = t;
                    break;
                }
            }

            // If this is an old binding, reset it, otherwise create a new one.
            if (gamepad_binding)
            {
                ResetGamepadBindings(binding, gamepad_binding, gamepad_index);
            } else {
                gamepad_binding = NewGamepadBinding(binding, gamepad_index);
                if (gamepad_binding)
                {
                    binding->m_GamepadBindings.Push(gamepad_binding);
                }
            }
        }
    }

    bool GamepadConnectivityCallback(uint32_t gamepad_index, bool connected, void* userdata)
    {
        Context* context = (Context*)userdata;
        Binding* binding = (Binding*)context->m_Binding;
        if (!binding)
        {
            context->m_UnmappedGamepads.Put(gamepad_index, connected);
            return true; // Only return false if you wish to override the controller connection status!
        }

        MapGamePad(binding, gamepad_index, connected);
        return true;
    }
}
