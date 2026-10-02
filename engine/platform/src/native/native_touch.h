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

#ifndef DM_PLATFORM_NATIVE_TOUCH_H
#define DM_PLATFORM_NATIVE_TOUCH_H

#define NATIVE_MAX_TOUCH (11)
#define NATIVE_PHASE_BEGAN (0)
#define NATIVE_PHASE_MOVED (1)
#define NATIVE_PHASE_STATIONARY (2)
#define NATIVE_PHASE_ENDED (3)
#define NATIVE_PHASE_CANCELLED (4)
#define NATIVE_PHASE_TAPPED (5)
#define NATIVE_PHASE_IDLE (6)

typedef struct {
    int TapCount;
    int Phase;
    int X;
    int Y;
    int DX;
    int DY;
    void* Reference;
    int Id;
} NativeTouch;

#ifdef __cplusplus
extern "C" {
#endif
// Mobile cancellation releases the remaining touches on the following update.
int dmNativeReadTouches(NativeTouch* states, NativeTouch* output, int capacity, int cancel_all);
#ifdef __cplusplus
}
#endif
#endif
