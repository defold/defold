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

#include "input.h"

extern "C"
{
    uint32_t InputActionBatchGetCount(HInputActionBatch batch)
    {
        return batch->m_Count;
    }

    HInputAction InputActionBatchGetAction(HInputActionBatch batch, uint32_t index)
    {
        return index < batch->m_Count ? &batch->m_Actions[index] : 0;
    }

    dmhash_t InputActionGetId(HInputAction action)
    {
        return action->m_Id;
    }

    float InputActionGetValue(HInputAction action)
    {
        return action->m_Value;
    }

    bool InputActionIsPressed(HInputAction action)
    {
        return action->m_Pressed;
    }

    bool InputActionIsReleased(HInputAction action)
    {
        return action->m_Released;
    }

    bool InputActionIsRepeated(HInputAction action)
    {
        return action->m_Repeated;
    }

    InputActionSource InputActionGetSource(HInputAction action)
    {
        return action->m_Source;
    }

    bool InputActionGetGamepadIndex(HInputAction action, uint32_t* out_index)
    {
        if (!action->m_IsGamepad)
        {
            return false;
        }
        *out_index = action->m_GamepadIndex;
        return true;
    }

}

namespace dmInput
{
    uint32_t ActionBatchGetCount(HActionBatch batch)
    {
        return InputActionBatchGetCount(batch);
    }

    HAction ActionBatchGetAction(HActionBatch batch, uint32_t index)
    {
        return InputActionBatchGetAction(batch, index);
    }

    dmhash_t ActionGetId(HAction action)
    {
        return InputActionGetId(action);
    }

    float ActionGetValue(HAction action)
    {
        return InputActionGetValue(action);
    }

    bool ActionIsPressed(HAction action)
    {
        return InputActionIsPressed(action);
    }

    bool ActionIsReleased(HAction action)
    {
        return InputActionIsReleased(action);
    }

    bool ActionIsRepeated(HAction action)
    {
        return InputActionIsRepeated(action);
    }

    ActionSource ActionGetSource(HAction action)
    {
        return InputActionGetSource(action);
    }

    bool ActionGetGamepadIndex(HAction action, uint32_t* out_index)
    {
        return InputActionGetGamepadIndex(action, out_index);
    }

}
