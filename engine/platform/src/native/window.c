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
#include <limits.h>

int dmNativeOpenWindow(const WindowCreateParams* params)
{
    if (!params || !dmNativeInitialized || dmNativeWin.opened)
    {
        return 0;
    }

    // Mobile apps may specify just one dimension when selecting the initial orientation.
    uint64_t width = params->m_Width;
    uint64_t height = params->m_Height;
    if (!width)
    {
        width = height ? height * 4 / 3 : 640;
    }

    if (!height)
    {
        height = width * 3 / 4;
    }

    if (width > INT_MAX || height > INT_MAX)
    {
        return 0;
    }

    int use_gl = params->m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL || params->m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGLES;
    dmNativeClearInput();
    dmNativeWin.active = 1;
    dmNativeWin.iconified = 0;
    dmNativeWin.width = (int)width;
    dmNativeWin.height = (int)height;
    dmNativeWin.opened = dmNativeOSOpenWindow((int)width, (int)height, use_gl ? NATIVE_OPENGL_API : NATIVE_NO_API);
    if (!dmNativeWin.opened || (use_gl && !dmNativeHasRequiredGLESVersion()))
    {
        dmNativeOSCloseWindow();
        memset(&dmNativeWin, 0, sizeof(dmNativeWin));
    }

    return dmNativeWin.opened;
}

void dmNativeCloseWindow(void)
{
    if (dmNativeWin.opened)
    {
        dmNativeOSCloseWindow();
    }

    memset(&dmNativeWin, 0, sizeof(dmNativeWin));
}

int dmNativeGetWindowParam(WindowState state)
{
    if (dmNativeWin.opened)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
            {
                return 1;
            }
            case WINDOW_STATE_ACTIVE:
            {
                return dmNativeWin.active;
            }
            case WINDOW_STATE_ICONIFIED:
            {
                return dmNativeWin.iconified;
            }
            default:
            {
                break;
            }
        }
    }

    return 0;
}

void dmNativeGetWindowSize(int* width, int* height)
{
    if (dmNativeWin.opened)
    {
        if (width)
        {
            *width = dmNativeWin.width;
        }

        if (height)
        {
            *height = dmNativeWin.height;
        }
    }
}

void dmNativeSetWindowSize(int width, int height)
{
    if (dmNativeWin.opened && !dmNativeWin.iconified)
    {
        dmNativeOSSetWindowSize(width, height);
    }
}

void dmNativeSetWindowSizeCallback(Nativewindowsizefun callback)
{
    if (dmNativeWin.opened)
    {
        dmNativeWin.windowSizeCallback = callback;
        if (callback)
        {
            callback(dmNativeWin.width, dmNativeWin.height);
        }
    }
}

void dmNativeSetWindowFocusCallback(Nativewindowfocusfun callback)
{
    if (dmNativeWin.opened)
    {
        dmNativeWin.windowFocusCallback = callback;
    }
}

// The mobile operating systems own the window title, position, and close and minimize actions.
void dmNativeSetWindowTitle(const char* title)
{
}

void dmNativeSetWindowPos(int x, int y)
{
}

void dmNativeSetWindowCloseCallback(Nativewindowclosefun callback)
{
}

void dmNativeSetWindowIconifyCallback(Nativewindowiconifyfun callback)
{
}

void dmNativeIconifyWindow(void)
{
    if (dmNativeWin.opened && !dmNativeWin.iconified)
    {
        dmNativeOSIconifyWindow();
    }
}

int dmNativeGetWindowRefreshRate(void)
{
    return dmNativeWin.opened ? dmNativeOSGetWindowRefreshRate() : 0;
}

void dmNativeSwapBuffers(void)
{
    if (dmNativeWin.opened)
    {
        dmNativeOSSwapBuffers();
        dmNativePollEvents();
    }
}

void dmNativePollEvents(void)
{
    if (dmNativeWin.opened)
    {
        dmNativeOSPollEvents();
    }
}

void dmNativeSwapInterval(int interval)
{
    if (dmNativeWin.opened)
    {
        dmNativeOSSwapInterval(interval);
    }
}

int dmNativeGetDefaultFramebuffer(void)
{
    return dmNativeOSGetDefaultFramebuffer();
}

int dmNativeQueryAuxContext(void)
{
    return dmNativeOSQueryAuxContext();
}

void* dmNativeAcquireAuxContext(void)
{
    return dmNativeOSAcquireAuxContext();
}

void dmNativeUnacquireAuxContext(void* context)
{
    dmNativeOSUnacquireAuxContext(context);
}

void dmNativeSetViewType(int type)
{
    dmNativeOSSetViewType(type);
}

void dmNativeSetWindowBackgroundColor(unsigned int color)
{
    dmNativeOSSetWindowBackgroundColor(color);
}

float dmNativeGetDisplayDensity(void)
{
    return dmNativeOSGetDisplayDensity();
}

float dmNativeGetDisplayScaleFactor(void)
{
    return dmNativeOSGetDisplayScaleFactor();
}

int dmNativeGetAcceleration(float* x, float* y, float* z)
{
    return dmNativeOSGetAcceleration(x, y, z);
}
