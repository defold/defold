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

#include <string.h>
#include <jc_test/jc_test.h>
#include "../native/native_touch.h"

// Verifies a tap between updates produces both press and release instead of losing the input.
TEST(NativeTouch, TapReportsBeganThenEnded)
{
    NativeTouch states[NATIVE_MAX_TOUCH] = {};
    NativeTouch output[NATIVE_MAX_TOUCH] = {};
    states[0].Reference = &states[0];
    states[0].Phase = NATIVE_PHASE_TAPPED;
    states[0].Id = 7;
    ASSERT_EQ(1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, 0));
    ASSERT_EQ(NATIVE_PHASE_BEGAN, output[0].Phase);
    ASSERT_EQ(7, output[0].Id);
    ASSERT_EQ(1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, 0));
    ASSERT_EQ(NATIVE_PHASE_ENDED, output[0].Phase);
    ASSERT_EQ(0, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, 0));
}

// Verifies mobile cancellation ends all touches while web cancellation affects only the cancelled touch.
TEST(NativeTouch, CancellationPreservesPlatformSemantics)
{
    for (int cancel_all = 0; cancel_all < 2; ++cancel_all)
    {
        NativeTouch states[NATIVE_MAX_TOUCH] = {};
        NativeTouch output[NATIVE_MAX_TOUCH] = {};
        states[0].Reference = &states[0];
        states[0].Phase = NATIVE_PHASE_CANCELLED;
        states[1].Reference = &states[1];
        states[1].Phase = NATIVE_PHASE_STATIONARY;
        ASSERT_EQ(2, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
        ASSERT_EQ(NATIVE_PHASE_CANCELLED, output[0].Phase);
        ASSERT_EQ(NATIVE_PHASE_STATIONARY, output[1].Phase);
        ASSERT_EQ(cancel_all ? 2 : 1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
        ASSERT_EQ(cancel_all ? NATIVE_PHASE_ENDED : NATIVE_PHASE_STATIONARY, output[0].Phase);
        ASSERT_EQ(cancel_all ? 0 : 1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
    }
}

// Verifies a small output buffer is respected and touches that did not fit keep their pending phase.
TEST(NativeTouch, CapacityPreservesPendingTouches)
{
    NativeTouch states[NATIVE_MAX_TOUCH] = {};
    NativeTouch output[2] = {};
    for (int i = 0; i < 2; ++i)
    {
        states[i].Reference = &states[i];
        states[i].Phase = NATIVE_PHASE_BEGAN;
    }
    output[1].Id = 12345;
    ASSERT_EQ(0, dmNativeReadTouches(states, output, 0, 0));
    ASSERT_EQ(NATIVE_PHASE_BEGAN, states[0].Phase);
    ASSERT_EQ(1, dmNativeReadTouches(states, output, 1, 0));
    ASSERT_EQ(12345, output[1].Id);
    ASSERT_EQ(NATIVE_PHASE_STATIONARY, states[0].Phase);
    ASSERT_EQ(NATIVE_PHASE_BEGAN, states[1].Phase);
}
