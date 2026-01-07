// Copyright 2020-2025 The Defold Foundation
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

#include <math.h>
#include <UIKit/UIKit.h>

#include "platform_window_ios.h"

namespace dmPlatform
{
    void SetiOSViewTypeOpenGL(HWindow window)
    {
        glfwSetViewType(GLFW_OPENGL_API);
    }

    id GetiOSUIWindow()
    {
        return glfwGetiOSUIWindow();
    }

    id GetiOSUIView()
    {
        return glfwGetiOSUIView();
    }

    id GetiOSEAGLContext()
    {
        return glfwGetiOSEAGLContext();
    }

    bool GetSafeAreaiOS(HWindow window, SafeArea* out)
    {
        UIView* view = (UIView*)glfwGetiOSUIView();
        if (!view || ![view respondsToSelector:@selector(safeAreaInsets)])
        {
            return false;
        }

        UIEdgeInsets insets = view.safeAreaInsets;
        const uint32_t window_width = GetWindowWidth(window);
        const uint32_t window_height = GetWindowHeight(window);

        // Convert UIKit points to pixel units to match window.get_size().
        CGFloat scale = view.contentScaleFactor;
        if (view.bounds.size.width > 0.0f)
        {
            scale = (CGFloat)window_width / view.bounds.size.width;
        }

        const int32_t inset_left = (int32_t)lrint(insets.left * scale);
        const int32_t inset_right = (int32_t)lrint(insets.right * scale);
        const int32_t inset_top = (int32_t)lrint(insets.top * scale);
        const int32_t inset_bottom = (int32_t)lrint(insets.bottom * scale);

        out->m_X = inset_left;
        out->m_Y = inset_bottom;
        out->m_Width = window_width - inset_left - inset_right;
        out->m_Height = window_height - inset_top - inset_bottom;
        out->m_InsetLeft = inset_left;
        out->m_InsetTop = inset_top;
        out->m_InsetRight = inset_right;
        out->m_InsetBottom = inset_bottom;

        return true;
    }

}
