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
#include "window.hpp"
#if defined(DM_PLATFORM_IOS)
#include <dlfcn.h>
#endif

int dmNativeHasRequiredGLESVersion(void)
{
#if defined(DM_PLATFORM_NO_OPENGL)
    return 0;
#elif defined(DM_PLATFORM_IOS)
    // EAGLView creates an ES 3 context and makes it current before returning.
    GLint major = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    return major >= 3;
#else
    // GL_MAJOR_VERSION is unavailable in ES 1 and 2. GL_VERSION exists in every ES context.
    return glGetString(GL_VERSION) != NULL;
#endif
}

namespace dmPlatform
{
    uintptr_t GetProcAddress(HWindow, const char* name)
    {
    #if defined(DM_PLATFORM_NO_OPENGL)
        return 0;
    #else
        if (!dmNativeWin.opened || !name)
        {
            return 0;
        }

    #if defined(DM_PLATFORM_IOS)
        return (uintptr_t)dlsym(RTLD_DEFAULT, name);
    #else
        return (uintptr_t)eglGetProcAddress(name);
    #endif
    #endif
    }

}
