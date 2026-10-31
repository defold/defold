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

int TestInputActionBatchC(HInputActionBatch batch)
{
    uint32_t count = InputActionBatchGetCount(batch);
    if (count == 0)
    {
        return InputActionBatchGetAction(batch, 0) == 0;
    }
    if (count != 3 || InputActionBatchGetAction(batch, count) != 0)
    {
        return 0;
    }
    HInputAction key = InputActionBatchGetAction(batch, 0);
    HInputAction pad = InputActionBatchGetAction(batch, 1);
    HInputAction release = InputActionBatchGetAction(batch, 2);
    uint32_t index = 77;
    if (InputActionGetGamepadIndex(key, &index) || index != 77)
    {
        return 0;
    }
    return InputActionGetId(key) == dmHashString64("up") &&
           InputActionGetValue(key) == 1.0f && InputActionIsPressed(key) &&
           !InputActionIsReleased(key) && !InputActionIsRepeated(key) &&
           InputActionGetSource(key) == INPUT_ACTION_SOURCE_KEYBOARD &&
           InputActionGetSource(pad) == INPUT_ACTION_SOURCE_GAMEPAD &&
           InputActionGetValue(pad) == 0.5f && InputActionIsRepeated(pad) &&
           InputActionGetGamepadIndex(pad, &index) && index == 2 &&
           InputActionGetValue(release) == 0.0f && InputActionIsReleased(release);
}
