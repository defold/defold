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

#include <dmsdk/input/input.h>

int TestInputActionIteratorC(HInputContext context, uint32_t expected_count, float value, bool pressed, bool released, bool repeated)
{
    InputActionIterator iterator = InputIterateActions(context);
    uint32_t count = 0;
    while (InputActionIterateNext(&iterator))
    {
        uint32_t index = 77;
        if (InputActionGetId(&iterator) != dmHashString64("up") ||
            InputActionGetValue(&iterator) != value ||
            InputActionIsPressed(&iterator) != pressed ||
            InputActionIsReleased(&iterator) != released ||
            InputActionIsRepeated(&iterator) != repeated ||
            InputActionGetSource(&iterator) != INPUT_ACTION_SOURCE_KEYBOARD ||
            InputActionGetGamepadIndex(&iterator, &index) || index != 77 ||
            InputActionGetGamepadNameHash(&iterator) ||
            InputActionIsGamepadConnected(&iterator) || InputActionIsGamepadDisconnected(&iterator))
        {
            return 0;
        }
        ++count;
    }
    return count == expected_count && !InputActionIterateNext(&iterator);
}

// Verify the canonical C metadata getters against a resolved gamepad action.
int TestInputGamepadMetadataC(HInputContext context, dmhash_t expected_name_hash, bool connected, bool disconnected)
{
    InputActionIterator iterator = InputIterateActions(context);
    uint32_t count = 0;
    while (InputActionIterateNext(&iterator))
    {
        dmhash_t name_hash = InputActionGetGamepadNameHash(&iterator);
        const char* id = connected ? "connected" : disconnected ? "disconnected" : "axis";
        uint32_t index = 77;
        if (InputActionGetId(&iterator) != dmHashString64(id) ||
            InputActionGetSource(&iterator) != INPUT_ACTION_SOURCE_GAMEPAD ||
            !InputActionGetGamepadIndex(&iterator, &index) || index != 0 ||
            InputActionIsGamepadConnected(&iterator) != connected ||
            InputActionIsGamepadDisconnected(&iterator) != disconnected ||
            name_hash != expected_name_hash)
        {
            return 0;
        }
        ++count;
    }
    return count == 1;
}
