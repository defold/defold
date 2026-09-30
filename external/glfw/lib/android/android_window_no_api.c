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

//========================================================================
// GLFW - An OpenGL framework
// Platform:    Android no-API window backend
// API version: 2.7
//========================================================================

#include "android_window_backend.h"
#include "android_util.h"

static int g_PendingResizeBecauseOfInsets = 0;

static int UpdateNoApiWindowSize(int force)
{
    ANativeWindow* window = glfwAcquireAndroidWindow();
    if (window)
    {
        int w = ANativeWindow_getWidth(window);
        int h = ANativeWindow_getHeight(window);
        glfwReleaseAndroidWindow(window);
        int changed = _glfwWin.width != w || _glfwWin.height != h;
        _glfwWin.width = w;
        _glfwWin.height = h;
        if ((force || changed) && _glfwWin.windowSizeCallback)
        {
            _glfwWin.windowSizeCallback(w, h);
        }
        return 1;
    }
    return 0;
}

int _glfwAndroidPlatformGetWindowRefreshRate(void)
{
    return 0;
}

int _glfwAndroidPlatformOpenWindow(int width, int height, const _GLFWwndconfig* wndconfig, const _GLFWfbconfig* fbconfig)
{
    (void)width;
    (void)height;
    (void)fbconfig;

    _glfwWin.clientAPI = wndconfig->clientAPI;
    return _glfwWin.clientAPI == GLFW_NO_API ? GL_TRUE : GL_FALSE;
}

void _glfwAndroidPlatformCloseWindow(void)
{
}

void _glfwAndroidPlatformSwapBuffers(void)
{
}

void _glfwAndroidPlatformSwapInterval(int interval)
{
    (void)interval;
}

int32_t _glfwAndroidPlatformVerifySurface(void)
{
    return 1;
}

void _glfwAndroidPlatformSetPendingResizeBecauseOfInsets(void)
{
    // The inset listener runs on the Android UI thread.
    __sync_lock_test_and_set(&g_PendingResizeBecauseOfInsets, 1);
}

void _glfwAndroidPlatformOnTermWindow(void)
{
}

void _glfwAndroidPlatformOnInitWindow(void)
{
    UpdateNoApiWindowSize(1);
}

void _glfwAndroidPlatformOnGainedFocus(void)
{
}

void _glfwAndroidPlatformOnResize(void)
{
    _glfwAndroidPlatformSetPendingResizeBecauseOfInsets();
}

void _glfwAndroidPlatformAfterFlushEvents(void)
{
    if (__sync_lock_test_and_set(&g_PendingResizeBecauseOfInsets, 0))
    {
        // Refresh insets even when a rotation leaves the window dimensions unchanged.
        if (!UpdateNoApiWindowSize(1))
        {
            _glfwAndroidPlatformSetPendingResizeBecauseOfInsets();
        }
    }
}

void _glfwAndroidPlatformDestroyWindow(void)
{
}

int _glfwAndroidPlatformQueryAuxContext(void)
{
    return 0;
}

void* _glfwAndroidPlatformAcquireAuxContext(void)
{
    return 0;
}

void _glfwAndroidPlatformUnacquireAuxContext(void* context)
{
    (void)context;
}
