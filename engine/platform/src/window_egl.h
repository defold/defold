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

#ifndef DM_WINDOW_EGL_H
#define DM_WINDOW_EGL_H

#include <dmsdk/platform/window.h>

struct WindowEGL;

namespace dmPlatform
{
    WindowEGL* EGLCreateWindow(void* native_window, const WindowCreateParams& params, uint32_t* sample_count);
    void       EGLDestroyWindow(WindowEGL* egl);
    uintptr_t  EGLGetProcAddress(WindowEGL* egl, const char* name);
    void       EGLSetSwapInterval(uint32_t interval);
    void       EGLSwapBuffers(WindowEGL* egl);
    void*      EGLAcquireAuxContext(WindowEGL* egl);
    void       EGLUnacquireAuxContext(WindowEGL* egl, void* context);
    bool       EGLHasAuxContext(WindowEGL* egl);
    int        EGLGetConfigParam(WindowEGL* egl, WindowState state);
}

#endif // DM_WINDOW_EGL_H
