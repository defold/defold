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

#include "android_window_backend.h"
#include "android_util.h"

static int g_PendingResizeBecauseOfInsets = 0;

static int UpdateNoApiWindowSize(int force)
{
    ANativeWindow* window = dmNativeAcquireAndroidWindow();
    if (window)
    {
        int w = ANativeWindow_getWidth(window);
        int h = ANativeWindow_getHeight(window);
        dmNativeReleaseAndroidWindow(window);
        int changed = dmNativeWin.width != w || dmNativeWin.height != h;
        dmNativeWin.width = w;
        dmNativeWin.height = h;
        if ((force || changed) && dmNativeWin.windowSizeCallback)
        {
            dmNativeWin.windowSizeCallback(w, h);
        }
        return 1;
    }
    return 0;
}

int dmNativeAndroidPlatformGetWindowRefreshRate(void)
{
    return 0;
}

int dmNativeAndroidPlatformOpenWindow(int client_api)
{
    dmNativeWin.clientAPI = client_api;
    return dmNativeWin.clientAPI == NATIVE_NO_API ? GL_TRUE : GL_FALSE;
}

void dmNativeAndroidPlatformCloseWindow(void)
{
}

void dmNativeAndroidPlatformSwapBuffers(void)
{
}

void dmNativeAndroidPlatformSwapInterval(int interval)
{
    (void)interval;
}

int32_t dmNativeAndroidPlatformVerifySurface(void)
{
    return 1;
}

void dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets(void)
{
    // The inset listener runs on the Android UI thread.
    __sync_lock_test_and_set(&g_PendingResizeBecauseOfInsets, 1);
}

void dmNativeAndroidPlatformOnTermWindow(void)
{
}

void dmNativeAndroidPlatformOnInitWindow(void)
{
    UpdateNoApiWindowSize(1);
}

void dmNativeAndroidPlatformOnGainedFocus(void)
{
}

void dmNativeAndroidPlatformOnResize(void)
{
    dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets();
}

void dmNativeAndroidPlatformAfterFlushEvents(void)
{
    if (__sync_lock_test_and_set(&g_PendingResizeBecauseOfInsets, 0))
    {
        // Refresh insets even when a rotation leaves the window dimensions unchanged.
        if (!UpdateNoApiWindowSize(1))
        {
            dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets();
        }
    }
}

void dmNativeAndroidPlatformDestroyWindow(void)
{
}

int dmNativeAndroidPlatformQueryAuxContext(void)
{
    return 0;
}

void* dmNativeAndroidPlatformAcquireAuxContext(void)
{
    return 0;
}

void dmNativeAndroidPlatformUnacquireAuxContext(void* context)
{
    (void)context;
}
