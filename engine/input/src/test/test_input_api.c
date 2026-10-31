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

int TestInputActionIteratorC(HInputContext context)
{
    InputActionIterator iterator = InputIterateActions(context);
    uint32_t count = 0;
    int valid = 1;
    while (InputActionIterateNext(&iterator))
    {
        uint32_t index = 77;
        switch (count)
        {
            case 0:
            {
                valid &= !InputActionGetGamepadIndex(&iterator, &index) && index == 77;
                valid &= InputActionGetId(&iterator) == dmHashString64("up") &&
                         InputActionGetValue(&iterator) == 1.0f && InputActionIsPressed(&iterator) &&
                         !InputActionIsReleased(&iterator) && !InputActionIsRepeated(&iterator) &&
                         InputActionGetSource(&iterator) == INPUT_ACTION_SOURCE_KEYBOARD;
                break;
            }
            case 1:
            {
                valid &= InputActionGetSource(&iterator) == INPUT_ACTION_SOURCE_GAMEPAD &&
                         InputActionGetValue(&iterator) == 0.5f && InputActionIsRepeated(&iterator) &&
                         InputActionGetGamepadIndex(&iterator, &index) && index == 2;
                break;
            }
            case 2:
            {
                valid &= InputActionGetValue(&iterator) == 0.0f && InputActionIsReleased(&iterator);
                break;
            }
            default:
            {
                valid = 0;
                break;
            }
        }
        ++count;
    }
    return valid && (count == 0 || count == 3) && !InputActionIterateNext(&iterator);
}
