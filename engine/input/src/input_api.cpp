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
#include "input_private.h"

static const dmInput::ResolvedAction* GetIteratorAction(const InputActionIterator* iterator)
{
    const dmInput::ResolvedActions* actions = (const dmInput::ResolvedActions*)iterator->m_State[0];
    return &actions->m_Actions[iterator->m_State[1] - 1];
}

extern "C"
{
    InputActionIterator InputIterateActions(HInputContext context)
    {
        InputActionIterator iterator = { { (uintptr_t)context->m_ResolvedActions, 0 } };
        return iterator;
    }

    bool InputActionIterateNext(InputActionIterator* iterator)
    {
        const dmInput::ResolvedActions* actions = (const dmInput::ResolvedActions*)iterator->m_State[0];
        if (actions && iterator->m_State[1] < actions->m_Count)
        {
            ++iterator->m_State[1];
            return true;
        }
        iterator->m_State[0] = 0;
        return false;
    }

    dmhash_t InputActionGetId(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_Id;
    }

    float InputActionGetValue(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_Value;
    }

    bool InputActionIsPressed(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_Pressed;
    }

    bool InputActionIsReleased(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_Released;
    }

    bool InputActionIsRepeated(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_Repeated;
    }

    InputActionSource InputActionGetSource(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_Source;
    }

    bool InputActionGetGamepadIndex(const InputActionIterator* iterator, uint32_t* out_index)
    {
        const dmInput::ResolvedAction* action = GetIteratorAction(iterator);
        if (!action->m_IsGamepad)
        {
            return false;
        }
        *out_index = action->m_GamepadIndex;
        return true;
    }

    const char* InputActionGetGamepadName(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_GamepadName;
    }

    bool InputActionIsGamepadConnected(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_GamepadConnected;
    }

    bool InputActionIsGamepadDisconnected(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_GamepadDisconnected;
    }
}

namespace dmInput
{
    const ResolvedActions* SetResolvedActions(HContext context, const ResolvedActions* actions)
    {
        const ResolvedActions* previous = context->m_ResolvedActions;
        context->m_ResolvedActions = actions;
        return previous;
    }

    ActionIterator IterateActions(HContext context)
    {
        return InputIterateActions(context);
    }

    bool ActionIterateNext(ActionIterator* iterator)
    {
        return InputActionIterateNext(iterator);
    }

    dmhash_t ActionGetId(const ActionIterator* iterator)
    {
        return InputActionGetId(iterator);
    }

    float ActionGetValue(const ActionIterator* iterator)
    {
        return InputActionGetValue(iterator);
    }

    bool ActionIsPressed(const ActionIterator* iterator)
    {
        return InputActionIsPressed(iterator);
    }

    bool ActionIsReleased(const ActionIterator* iterator)
    {
        return InputActionIsReleased(iterator);
    }

    bool ActionIsRepeated(const ActionIterator* iterator)
    {
        return InputActionIsRepeated(iterator);
    }

    ActionSource ActionGetSource(const ActionIterator* iterator)
    {
        return InputActionGetSource(iterator);
    }

    bool ActionGetGamepadIndex(const ActionIterator* iterator, uint32_t* out_index)
    {
        return InputActionGetGamepadIndex(iterator, out_index);
    }

    const char* ActionGetGamepadName(const ActionIterator* iterator)
    {
        return InputActionGetGamepadName(iterator);
    }

    bool ActionIsGamepadConnected(const ActionIterator* iterator)
    {
        return InputActionIsGamepadConnected(iterator);
    }

    bool ActionIsGamepadDisconnected(const ActionIterator* iterator)
    {
        return InputActionIsGamepadDisconnected(iterator);
    }
}
