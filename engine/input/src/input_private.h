// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_INPUT_PRIVATE_H
#define DM_INPUT_PRIVATE_H

#include <dlib/hashtable.h>
#include <dlib/index_pool.h>

#include <input/input_ddf.h>

namespace dmInput
{
    struct KeyTrigger
    {
        dmInputDDF::Key m_Input;
        dmhash_t m_ActionId;
    };

    struct KeyboardBinding
    {
        dmHID::HKeyboard      m_Keyboard;
        dmHID::KeyboardPacket m_PreviousPacket;
        dmHID::KeyboardPacket m_Packet;
        dmArray<KeyTrigger> m_Triggers;
    };

    struct MouseTrigger
    {
        dmInputDDF::Mouse m_Input;
        dmhash_t m_ActionId;
    };

    struct MouseBinding
    {
        dmHID::HMouse           m_Mouse;
        dmHID::MousePacket      m_PreviousPacket;
        dmHID::MousePacket      m_Packet;
        dmArray<MouseTrigger>   m_Triggers;
    };

    struct GamepadTrigger
    {
        dmInputDDF::Gamepad m_Input;
        dmhash_t m_ActionId;
    };

    struct GamepadBinding
    {
        dmHID::HGamepad m_Gamepad;
        dmHID::GamepadPacket m_PreviousPacket;
        dmHID::GamepadPacket m_Packet;
        dmArray<GamepadTrigger> m_Triggers;
        dmHashTable64< Action > m_Actions;
        uint32_t m_DeviceId;
        uint8_t m_Index;
        uint8_t m_Connected : 1;
        uint8_t m_Unknown : 1;
        uint8_t m_NoMapWarning : 1;
    };

    struct TouchTrigger
    {
        dmInputDDF::Touch m_Input;
        dmhash_t m_ActionId;
    };

    struct TouchDeviceBinding
    {
        dmHID::HTouchDevice      m_TouchDevice;
        dmHID::TouchDevicePacket m_PreviousPacket;
        dmHID::TouchDevicePacket m_Packet;
        dmArray<TouchTrigger> m_Triggers;
    };

    struct TextTrigger
    {
        dmInputDDF::Text m_Input;
        dmhash_t m_ActionId;
    };

    struct TextBinding
    {
        dmHID::TextPacket m_TextPacket;
        dmHID::MarkedTextPacket m_MarkedTextPacket;
        dmArray<TextTrigger> m_Triggers;
    };

    struct AccelerationBinding
    {
        dmHID::AccelerationPacket m_PreviousPacket;
        dmHID::AccelerationPacket m_Packet;
    };

    struct Binding
    {
        Context*                    m_Context;
        KeyboardBinding*            m_KeyboardBinding;
        MouseBinding*               m_MouseBinding;
        dmArray<GamepadBinding*>    m_GamepadBindings;
        TouchDeviceBinding*         m_TouchDeviceBinding;
        AccelerationBinding*        m_AccelerationBinding;
        TextBinding*                m_TextBinding;
        dmHashTable64<Action>       m_Actions;
        dmArray<uint32_t>           m_DisconnectedGamepadIndices;
        dmInputDDF::GamepadTrigger* m_DDFGamepadTriggersData;
        uint32_t                    m_DDFGamepadTriggersCount;
    };

    struct GamepadInput
    {
        uint16_t m_Index;
        uint16_t m_HatMask;
        uint16_t m_Type : 2;
        uint16_t m_Negate : 1;
        uint16_t m_Scale : 1;
        uint16_t m_Clamp : 1;
    };

    struct GamepadConfig
    {
        uint32_t m_DeviceId;
        float m_DeadZone;
        GamepadInput m_Inputs[dmInputDDF::MAX_GAMEPAD_COUNT];
    };

    struct Context
    {
        dmIndexPool8                    m_GamepadIndices;
        dmHashTable32< GamepadConfig >  m_GamepadMaps;
        dmHashTable32<bool>             m_UnmappedGamepads;
        dmHID::HContext                 m_HidContext;
        Binding*                        m_Binding;
        float                           m_RepeatDelay;
        float                           m_RepeatInterval;
        float                           m_PressedThreshold;
    };
}

#endif // DM_INPUT_PRIVATE_H
