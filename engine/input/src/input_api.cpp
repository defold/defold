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


static dmInput::InputAction* GetIteratorAction(const InputActionIterator* iterator)
{
    dmInput::Context* context = (dmInput::Context*)iterator->m_State[0];
    return &context->m_InputActions[iterator->m_State[1] - 1];
}

static InputActionSource GetInputActionSource(dmHID::InputSource source)
{
    switch (source)
    {
        case dmHID::INPUT_SOURCE_KEYBOARD:
        {
            return INPUT_ACTION_SOURCE_KEYBOARD;
        }
        case dmHID::INPUT_SOURCE_TEXT:
        {
            return INPUT_ACTION_SOURCE_TEXT;
        }
        case dmHID::INPUT_SOURCE_MOUSE:
        {
            return INPUT_ACTION_SOURCE_MOUSE;
        }
        case dmHID::INPUT_SOURCE_TOUCH:
        {
            return INPUT_ACTION_SOURCE_TOUCH;
        }
        case dmHID::INPUT_SOURCE_GAMEPAD:
        {
            return INPUT_ACTION_SOURCE_GAMEPAD;
        }
        case dmHID::INPUT_SOURCE_ACCELEROMETER:
        {
            return INPUT_ACTION_SOURCE_ACCELEROMETER;
        }
        default:
        {
            return INPUT_ACTION_SOURCE_UNKNOWN;
        }
    }
}

extern "C"
{
    InputActionIterator InputIterateActions(HInputContext context)
    {
        InputActionIterator iterator = { { (uintptr_t)context, 0 } };
        return iterator;
    }

    bool InputActionIterateNext(InputActionIterator* iterator)
    {
        const dmInput::Context* context = (const dmInput::Context*)iterator->m_State[0];
        if (context && iterator->m_State[1] < context->m_InputActions.Size())
        {
            ++iterator->m_State[1];
            return true;
        }
        iterator->m_State[0] = 0;
        return false;
    }

    dmhash_t InputActionGetId(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_ActionId;
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
        return GetInputActionSource((dmHID::InputSource)GetIteratorAction(iterator)->m_Source);
    }

    bool InputActionGetGamepadIndex(const InputActionIterator* iterator, uint32_t* out_index)
    {
        if (!GetIteratorAction(iterator)->m_IsGamepad)
        {
            return false;
        }
        *out_index = GetIteratorAction(iterator)->m_GamepadIndex;
        return true;
    }

    dmhash_t InputActionGetGamepadNameHash(const InputActionIterator* iterator)
    {
        return GetIteratorAction(iterator)->m_GamepadNameHash;
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
    const InputAction* GetInputAction(const ActionIterator* iterator)
    {
        return GetIteratorAction(iterator);
    }

    void ConsumeAction(const ActionIterator* iterator)
    {
        GetIteratorAction(iterator)->m_Consumed = 1;
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

    dmhash_t ActionGetGamepadNameHash(const ActionIterator* iterator)
    {
        return InputActionGetGamepadNameHash(iterator);
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
