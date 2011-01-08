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
        uint32_t m_Pressed : 1;
        uint32_t m_Released : 1;
        uint32_t m_Repeated : 1;
    };

    typedef struct Context* HContext;
    /**
     * Invalid context handle
     */
    const HContext INVALID_CONTEXT = 0;

    typedef struct Binding* HBinding;
    /**
     * Invalid binding handle
     */
    const HBinding INVALID_BINDING = 0;

    HContext NewContext(float repeat_delay, float repeat_interval);
    void DeleteContext(HContext context);
    void SetRepeat(HContext context, float delay, float interval);

    HBinding NewBinding(HContext context, dmInputDDF::InputBinding* binding);
    void DeleteBinding(HBinding binding);

    void RegisterGamepads(HContext context, const dmInputDDF::GamepadMaps* ddf);

    void UpdateBinding(HBinding binding, float dt);

    float GetValue(HBinding binding, dmhash_t action_id);
    bool Pressed(HBinding binding, dmhash_t action_id);
    bool Released(HBinding binding, dmhash_t action_id);
    bool Repeated(HBinding binding, dmhash_t action_id);

    typedef void (*ActionCallback)(dmhash_t action_id, Action* action, void* user_data);

    void ForEachActive(HBinding binding, ActionCallback callback, void* user_data);
}

#endif // DM_INPUT_H
