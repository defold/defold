// Copyright 2020-2024 The Defold Foundation
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

namespace dmPlatform
{
    struct Window
    {
        GLFWwindow*                   m_Window;
        GLFWwindow*                   m_AuxWindow;

        WindowResizeCallback          m_ResizeCallback;
        void*                         m_ResizeCallbackUserData;
        WindowCloseCallback           m_CloseCallback;
        void*                         m_CloseCallbackUserData;
        WindowFocusCallback           m_FocusCallback;
        void*                         m_FocusCallbackUserData;
        WindowIconifyCallback         m_IconifyCallback;
        void*                         m_IconifyCallbackUserData;
        WindowAddKeyboardCharCallback m_AddKeyboarCharCallBack;
        void*                         m_AddKeyboarCharCallBackUserData;
        WindowSetMarkedTextCallback   m_SetMarkedTextCallback;
        void*                         m_SetMarkedTextCallbackUserData;
        WindowDeviceChangedCallback   m_DeviceChangedCallback;
        void*                         m_DeviceChangedCallbackUserData;
        double                        m_MouseScrollX;
        double                        m_MouseScrollY;
        int32_t                       m_Width;
        int32_t                       m_Height;
        uint32_t                      m_Samples               : 8;
        uint32_t                      m_HighDPI               : 1;
        uint32_t                      m_SwapIntervalSupported : 1;
        uint32_t                      m_WindowOpened          : 1;
    };
}

#endif // DM_PLATFORM_WINDOW_GLFW3_PRIVATE_H
