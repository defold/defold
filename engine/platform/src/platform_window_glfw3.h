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

#ifndef DM_PLATFORM_WINDOW_GLFW3_H
#define DM_PLATFORM_WINDOW_GLFW3_H

#include "window.hpp"

struct GLFWwindow;

// Native window interop for GLFW3 platform builds.
namespace dmPlatform
{
    // Returns the GLFW3 handle for an open window. The handle is borrowed until
    // CloseWindow(); callers must not destroy it. Call on the platform thread.
    GLFWwindow* GetGLFWWindow(HWindow window);
}

#endif // DM_PLATFORM_WINDOW_GLFW3_H
