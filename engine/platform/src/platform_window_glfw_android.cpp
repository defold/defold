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

#include <glfw/glfw.h>

#include <glfw/glfw_native.h>

#include <dlib/math.h>

#include "platform_window_android.h"

namespace dmPlatform
{
    extern "C" int _glfwAndroidGetSafeAreaInsets(int* left, int* top, int* right, int* bottom);

    int32_t AndroidVerifySurface(HWindow window)
    {
        return glfwAndroidVerifySurface();
    }

    void AndroidBeginFrame(HWindow window)
    {
        glfwAndroidBeginFrame();
    }

    EGLContext GetAndroidEGLContext()
    {
        return glfwGetAndroidEGLContext();
    }

    EGLSurface GetAndroidEGLSurface()
    {
        return glfwGetAndroidEGLSurface();
    }

    JavaVM* GetAndroidJavaVM()
    {
        return glfwGetAndroidJavaVM();
    }

    jobject GetAndroidActivity()
    {
        return glfwGetAndroidActivity();
    }

    android_app* GetAndroidApp()
    {
        return glfwGetAndroidApp();
    }

    bool GetSafeAreaAndroid(HWindow window, SafeArea* out)
    {
        const int32_t window_width = (int32_t) GetWindowWidth(window);
        const int32_t window_height = (int32_t) GetWindowHeight(window);
        if (window_width <= 0 || window_height <= 0)
        {
            return false;
        }

        int32_t inset_left = 0;
        int32_t inset_top = 0;
        int32_t inset_right = 0;
        int32_t inset_bottom = 0;

        if (_glfwAndroidGetSafeAreaInsets(&inset_left, &inset_top, &inset_right, &inset_bottom))
        {
            out->m_X = inset_left;
            out->m_Y = inset_bottom;
            out->m_Width = (uint32_t) dmMath::Max(0, window_width - inset_left - inset_right);
            out->m_Height = (uint32_t) dmMath::Max(0, window_height - inset_top - inset_bottom);
            out->m_InsetLeft = inset_left;
            out->m_InsetTop = inset_top;
            out->m_InsetRight = inset_right;
            out->m_InsetBottom = inset_bottom;
            return true;
        }

        android_app* app = glfwGetAndroidApp();
        // ARect content depends on window settings we swt in DefoldActivity.java
        const ARect rect = app->contentRect;
        const int32_t rect_width = rect.right - rect.left;
        const int32_t rect_height = rect.bottom - rect.top;
        if (rect_width <= 0 || rect_height <= 0)
        {
            return false;
        }

        inset_left = rect.left;
        inset_right = window_width - rect.right;
        inset_top = rect.top;
        inset_bottom = window_height - rect.bottom;

        out->m_X = rect.left;
        out->m_Y = window_height - rect.bottom;
        out->m_Width = (uint32_t) rect_width;
        out->m_Height = (uint32_t) rect_height;
        out->m_InsetLeft = inset_left;
        out->m_InsetTop = inset_top;
        out->m_InsetRight = inset_right;
        out->m_InsetBottom = inset_bottom;

        return true;
    }
}
