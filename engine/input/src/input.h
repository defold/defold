#ifndef DM_INPUT_H
#define DM_INPUT_H

#include <ddf/ddf.h>
#include <dlib/hash.h>
#include <hid/hid.h>

#include "input_ddf.h"

namespace dmInput
{
    struct Action
    {
        float m_Value;
        float m_PrevValue;
        float m_RepeatTimer;
        int32_t m_X;
        int32_t m_Y;
        int32_t m_DX;
        int32_t m_DY;
        float m_AccX;
        float m_AccY;
        float m_AccZ;
        dmHID::Touch m_Touch[dmHID::MAX_TOUCH_COUNT];
        int32_t      m_TouchCount;
        char         m_Text[dmHID::MAX_CHAR_COUNT];
        uint32_t     m_TextCount;
        uint32_t     m_HasText;
        uint32_t m_GamepadIndex;
        uint32_t m_IsGamepad : 1;
        uint32_t m_HasConnectivity : 1;
        uint32_t m_Connected : 1;
        uint32_t m_Pressed : 1;
        uint32_t m_Released : 1;
        uint32_t m_Repeated : 1;
        uint32_t m_PositionSet : 1;
        uint32_t m_AccelerationSet : 1;
    };

    typedef struct Context* HContext;
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
    };

    HContext NewContext(const NewContextParams& params);
    void DeleteContext(HContext context);
    void SetRepeat(HContext context, float delay, float interval);

    HBinding NewBinding(HContext context);
    void SetBinding(HBinding binding, dmInputDDF::InputBinding* ddf);
    void DeleteBinding(HBinding binding);

    void RegisterGamepads(HContext context, const dmInputDDF::GamepadMaps* ddf);

    void UpdateBinding(HBinding binding, float dt);

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

    void ForEachActive(HBinding binding, ActionCallback callback, void* user_data);
    void GamepadConnectivityCallback(uint32_t gamepad_index, bool connected, void* context);
}

#endif // DM_INPUT_H
