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

// Verifies cancellation beyond the output capacity releases all touches instead of repeatedly reporting the first touch as ended.
TEST(NativeTouch, CancellationBeyondCapacityReleasesAllTouches)
{
    NativeTouch states[NATIVE_MAX_TOUCH] = {};
    NativeTouch output[2] = {};
    states[0].Reference = &states[0];
    states[0].Phase = NATIVE_PHASE_STATIONARY;
    states[0].Id = 7;
    states[NATIVE_MAX_TOUCH - 1].Reference = &states[NATIVE_MAX_TOUCH - 1];
    states[NATIVE_MAX_TOUCH - 1].Phase = NATIVE_PHASE_CANCELLED;
    states[NATIVE_MAX_TOUCH - 1].Id = 8;
    output[1].Id = 12345;

    ASSERT_EQ(1, dmNativeReadTouches(states, output, 1, 1));
    ASSERT_EQ(NATIVE_PHASE_STATIONARY, output[0].Phase);
    ASSERT_EQ(7, output[0].Id);
    ASSERT_EQ(1, dmNativeReadTouches(states, output, 1, 1));
    ASSERT_EQ(NATIVE_PHASE_ENDED, output[0].Phase);
    ASSERT_EQ(7, output[0].Id);
    ASSERT_EQ(1, dmNativeReadTouches(states, output, 1, 1));
    ASSERT_EQ(NATIVE_PHASE_ENDED, output[0].Phase);
    ASSERT_EQ(8, output[0].Id);
    ASSERT_EQ(0, dmNativeReadTouches(states, output, 1, 1));
    ASSERT_EQ(12345, output[1].Id);
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

// Verifies press and move deltas are reported once, guarding against stale deltas on frames without touch events (#9411).
TEST(NativeTouch, DeltasAreConsumedOnce)
{
    const int phases[] = {NATIVE_PHASE_BEGAN, NATIVE_PHASE_MOVED};
    for (int cancel_all = 0; cancel_all < 2; ++cancel_all)
    {
        for (int phase_index = 0; phase_index < 2; ++phase_index)
        {
            NativeTouch states[NATIVE_MAX_TOUCH] = {};
            NativeTouch output[NATIVE_MAX_TOUCH] = {};
            NativeTouch* touch = &states[0];
            touch->Reference = touch;
            touch->Phase = phases[phase_index];
            touch->Id = 7;
            touch->X = 103;
            touch->Y = 198;
            touch->DX = 3;
            touch->DY = -2;

            ASSERT_EQ(1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
            ASSERT_EQ(phases[phase_index], output[0].Phase);
            ASSERT_EQ(3, output[0].DX);
            ASSERT_EQ(-2, output[0].DY);

            for (int frame = 0; frame < 2; ++frame)
            {
                ASSERT_EQ(1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
                ASSERT_EQ(7, output[0].Id);
                ASSERT_EQ(103, output[0].X);
                ASSERT_EQ(198, output[0].Y);
                ASSERT_EQ(0, output[0].DX);
                ASSERT_EQ(0, output[0].DY);
            }

            touch->Phase = NATIVE_PHASE_MOVED;
            touch->X = 107;
            touch->Y = 197;
            touch->DX = 4;
            touch->DY = -1;
            ASSERT_EQ(1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
            ASSERT_EQ(NATIVE_PHASE_MOVED, output[0].Phase);
            ASSERT_EQ(107, output[0].X);
            ASSERT_EQ(197, output[0].Y);
            ASSERT_EQ(4, output[0].DX);
            ASSERT_EQ(-1, output[0].DY);
        }
    }
}

// Verifies touches outside the output capacity keep their pending deltas until read instead of losing movement.
TEST(NativeTouch, CapacityPreservesPendingDeltas)
{
    for (int cancel_all = 0; cancel_all < 2; ++cancel_all)
    {
        NativeTouch states[NATIVE_MAX_TOUCH] = {};
        NativeTouch output[NATIVE_MAX_TOUCH] = {};
        for (int i = 0; i < 2; ++i)
        {
            states[i].Reference = &states[i];
            states[i].Phase = NATIVE_PHASE_MOVED;
            states[i].DX = 3 * (i + 1);
            states[i].DY = -2 * (i + 1);
        }

        ASSERT_EQ(0, dmNativeReadTouches(states, output, 0, cancel_all));
        ASSERT_EQ(1, dmNativeReadTouches(states, output, 1, cancel_all));
        ASSERT_EQ(3, output[0].DX);
        ASSERT_EQ(-2, output[0].DY);
        ASSERT_EQ(1, dmNativeReadTouches(states, output, 1, cancel_all));
        ASSERT_EQ(0, output[0].DX);
        ASSERT_EQ(0, output[0].DY);
        ASSERT_EQ(2, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
        ASSERT_EQ(0, output[0].DX);
        ASSERT_EQ(0, output[0].DY);
        ASSERT_EQ(6, output[1].DX);
        ASSERT_EQ(-4, output[1].DY);
    }
}

// Verifies a tap's synthetic release does not repeat movement already reported with the press (#9411).
TEST(NativeTouch, TapDeltaIsConsumedOnce)
{
    for (int cancel_all = 0; cancel_all < 2; ++cancel_all)
    {
        NativeTouch states[NATIVE_MAX_TOUCH] = {};
        NativeTouch output[NATIVE_MAX_TOUCH] = {};
        states[0].Reference = &states[0];
        states[0].Phase = NATIVE_PHASE_TAPPED;
        states[0].DX = 3;
        states[0].DY = -2;
        ASSERT_EQ(1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
        ASSERT_EQ(NATIVE_PHASE_BEGAN, output[0].Phase);
        ASSERT_EQ(3, output[0].DX);
        ASSERT_EQ(-2, output[0].DY);
        ASSERT_EQ(1, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
        ASSERT_EQ(NATIVE_PHASE_ENDED, output[0].Phase);
        ASSERT_EQ(0, output[0].DX);
        ASSERT_EQ(0, output[0].DY);
        ASSERT_EQ(0, dmNativeReadTouches(states, output, NATIVE_MAX_TOUCH, cancel_all));
    }
}
