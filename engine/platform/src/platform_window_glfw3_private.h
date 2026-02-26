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

#ifndef DM_PLATFORM_WINDOW_GLFW3_PRIVATE_H
#define DM_PLATFORM_WINDOW_GLFW3_PRIVATE_H

#include <glfw/glfw3.h>

#include "window.hpp"

struct dmWindow
{
    GLFWwindow*                    m_Window;
    GLFWwindow*                    m_AuxWindow;

    FWindowResizeCallback          m_ResizeCallback;
    void*                          m_ResizeCallbackUserData;
    FWindowCloseCallback           m_CloseCallback;
    void*                          m_CloseCallbackUserData;
    FWindowFocusCallback           m_FocusCallback;
    void*                          m_FocusCallbackUserData;
    FWindowIconifyCallback         m_IconifyCallback;
    void*                          m_IconifyCallbackUserData;
    FWindowAddKeyboardCharCallback m_AddKeyboarCharCallBack;
    void*                          m_AddKeyboarCharCallBackUserData;
    FWindowSetMarkedTextCallback   m_SetMarkedTextCallback;
    void*                          m_SetMarkedTextCallbackUserData;
    FWindowDeviceChangedCallback   m_DeviceChangedCallback;
    void*                          m_DeviceChangedCallbackUserData;
    double                         m_MouseScrollX;
    double                         m_MouseScrollY;
    float                          m_XScale;
    float                          m_YScale;
    int32_t                        m_Width;
    int32_t                        m_Height;
    int32_t                        m_WidthScreen;
    int32_t                        m_HeightScreen;
    uint32_t                       m_Samples               : 8;
    uint32_t                       m_HighDPI               : 1;
    uint32_t                       m_SwapIntervalSupported : 1;
    uint32_t                       m_WindowOpened          : 1;
};

namespace dmPlatform
{
    void FocusWindowNative(HWindow window);
    void CenterWindowNative(HWindow wnd, GLFWmonitor* monitor);
    void SetWindowsIconNative(HWindow window);
}

#endif // DM_PLATFORM_WINDOW_GLFW3_PRIVATE_H
