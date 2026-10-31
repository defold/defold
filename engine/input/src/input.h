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

#ifndef DM_INPUT_H
#define DM_INPUT_H

#include <ddf/ddf.h>
#include <dlib/hash.h>
#include <hid/hid.h>

#include <input/input_ddf.h>
#include <dmsdk/input/input.h>

namespace dmInput
{
    // Private resolved views; independent of the public iterator and focused input buffer.
    struct ResolvedAction
    {
        dmhash_t     m_Id;
        float        m_Value;
        ActionSource m_Source;
        uint32_t     m_GamepadIndex;
        bool         m_Pressed;
        bool         m_Released;
        bool         m_Repeated;
        bool         m_IsGamepad;
        bool         m_GamepadConnected;
        bool         m_GamepadDisconnected;
        const char*  m_GamepadName;
    };

    struct ResolvedActions
    {
        const ResolvedAction* m_Actions;
        uint32_t              m_Count;
    };

    // Publish a callback-scoped view; return the previous view for nested delivery.
    const ResolvedActions* SetResolvedActions(HContext context, const ResolvedActions* actions);

    // TODO: Make proper subtypes, to keep the combinations apart, and struct small
    struct Action
    {
        union {
            dmHID::Touch         m_Touch[dmHID::MAX_TOUCH_COUNT];
            char                 m_Text[dmHID::MAX_CHAR_COUNT];
        };
        dmHID::GamepadPacket m_GamepadPacket;
        dmHID::GamepadGuid   m_GamepadGuid; // Valid when m_GamepadConnected == 1
        float m_Value;
        float m_PrevValue;
        float m_RepeatTimer;
        float m_AccX;
        float m_AccY;
        float m_AccZ;
        int16_t m_X;
        int16_t m_Y;
        int16_t m_DX;
        int16_t m_DY;

        /// Text or touch count
        int16_t  m_Count;
        uint16_t m_GamepadIndex;
        uint16_t m_UserID;

        uint16_t m_IsGamepad : 1;
        uint16_t m_GamepadUnknown : 1;
        uint16_t m_GamepadDisconnected : 1;
        uint16_t m_GamepadConnected : 1;
        uint16_t m_HasGamepadPacket : 1;
        uint16_t m_Pressed : 1;
        uint16_t m_Released : 1;
        uint16_t m_Repeated : 1;
        uint16_t m_PositionSet : 1;
        uint16_t m_AccelerationSet : 1;
        uint16_t m_HasText : 1;
        uint16_t m_Dirty : 1; // it's dirty and should report its value
        uint16_t m_Source : 3; // dmHID::InputSource
        uint16_t : 1;
    };

    /**
     * Invalid context handle
     */
    const HContext INVALID_CONTEXT = 0;

    typedef struct Binding* HBinding;
    typedef struct GamepadBinding* HGamepadBinding;
    /**
     * Invalid binding handle
     */
    const HBinding INVALID_BINDING = 0;

    struct NewContextParams
    {
        dmHID::HContext m_HidContext;
        float m_RepeatDelay;
        float m_RepeatInterval;
        float m_GamepadDeadZone;
    };

    HContext NewContext(const NewContextParams& params);
    void DeleteContext(HContext context);
    void Update(HContext context);
    void SetRepeat(HContext context, float delay, float interval);

    HBinding NewBinding(HContext context);
    void SetBinding(HBinding binding, dmInputDDF::InputBinding* ddf);
    void DeleteBinding(HBinding binding);

    void RegisterGamepads(HContext context, const dmInputDDF::GamepadMapsRuntime* ddf);

    void UpdateBinding(HBinding binding, float dt);

    // Action id 0 refers to pointer movement. Accelerometer samples are delivered by ForEachActive.
    const Action* GetAction(HBinding binding, dmhash_t action_id);
    float GetValue(HBinding binding, dmhash_t action_id);
    bool Pressed(HBinding binding, dmhash_t action_id);
    bool Released(HBinding binding, dmhash_t action_id);
    bool Repeated(HBinding binding, dmhash_t action_id);
    float GetValue(HGamepadBinding binding, dmhash_t action_id);
    bool Pressed(HGamepadBinding binding, dmhash_t action_id);
    bool Released(HGamepadBinding binding, dmhash_t action_id);
    bool Repeated(HGamepadBinding binding, dmhash_t action_id);

    typedef void (*ActionCallback)(dmhash_t action_id, Action* action, void* user_data);

    // Pointer movement and accelerometer samples are separate callbacks, both with action id 0.
    void ForEachActive(HBinding binding, ActionCallback callback, void* user_data);

    // Last known mapping-aware name, or null when unavailable. The binding owns
    // the string and may replace it on reconnect or reload.
    const char* GetGamepadName(HBinding binding, uint32_t gamepad_index);

    bool GamepadConnectivityCallback(uint32_t gamepad_index, bool connected, void* context);
}

#endif // DM_INPUT_H
