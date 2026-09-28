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

#include "native_touch.h"

int dmNativeReadTouches(NativeTouch* states, NativeTouch* output, int capacity, int cancel_all)
{
    int cancelled = 0;
    if (cancel_all)
    {
        for (int i = 0; i < NATIVE_MAX_TOUCH; ++i)
            cancelled |= states[i].Reference && states[i].Phase == NATIVE_PHASE_CANCELLED;
    }

    int count = 0;
    for (int i = 0; i < NATIVE_MAX_TOUCH && count < capacity; ++i)
    {
        NativeTouch* touch = &states[i];
        if (!touch->Reference)
            continue;

        output[count] = *touch;
        if (cancelled)
            touch->Phase = NATIVE_PHASE_ENDED;
        else if (touch->Phase == NATIVE_PHASE_ENDED || touch->Phase == NATIVE_PHASE_CANCELLED)
        {
            touch->Reference = 0;
            touch->Phase = NATIVE_PHASE_IDLE;
        }
        else if (touch->Phase == NATIVE_PHASE_BEGAN)
            touch->Phase = NATIVE_PHASE_STATIONARY;
        else if (touch->Phase == NATIVE_PHASE_TAPPED)
        {
            output[count].Phase = NATIVE_PHASE_BEGAN;
            touch->Phase = NATIVE_PHASE_ENDED;
        }
        ++count;
    }
    return count;
}
