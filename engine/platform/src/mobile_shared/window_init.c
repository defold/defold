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

#include "window_mobile_private.h"

Nativewin dmNativeWin;
int dmNativeInitialized;
static int g_RegisteredTermination;

#if defined(ANDROID)
Nativewin_android dmNativeWinAndroid;
NativeAndroidJoystick dmNativeJoy[NATIVE_JOYSTICK_LAST + 1];
#endif

int dmNativeInit(void)
{
    if (!dmNativeInitialized)
    {
        dmNativeInitialized = dmNativeOSInit();
    }

    if (dmNativeInitialized && !g_RegisteredTermination)
    {
        g_RegisteredTermination = atexit(dmNativeTerminate) == 0;
    }

    return dmNativeInitialized;
}

void dmNativeCloseWindow(void)
{
    if (dmNativeWin.opened)
    {
        dmNativeOSCloseWindow();
    }

    memset(&dmNativeWin, 0, sizeof(dmNativeWin));
}

void dmNativeTerminate(void)
{
    if (dmNativeInitialized && dmNativeOSTerminate())
    {
        dmNativeInitialized = 0;
    }
}
