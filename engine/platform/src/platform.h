// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_PLATFORM_H
#define DM_PLATFORM_H

#include <stdint.h>

namespace dmPlatform
{
    typedef void* HWindow;
    typedef void (*WindowResizeCallback)(void* user_data, uint32_t width, uint32_t height);
    typedef void (*WindowFocusCallback)(void* user_data, uint32_t focus);
    typedef void (*WindowIconifyCallback)(void* user_data, uint32_t iconified);
    typedef bool (*WindowCloseCallback)(void* user_data);

    enum PlatformResult
    {
        PLATFORM_RESULT_OK                    = 0,
        PLATFORM_RESULT_WINDOW_OPEN_ERROR     = -1,
        PLATFORM_RESULT_WINDOW_ALREADY_OPENED = -2,
    };

    enum PlatformGraphicsApi
    {
        PLATFORM_GRAPHICS_API_NULL   = 0,
        PLATFORM_GRAPHICS_API_OPENGL = 1,
        PLATFORM_GRAPHICS_API_VULKAN = 2,
    };

    enum WindowState
    {
        WINDOW_STATE_UNKNOWN            = 0,
        WINDOW_STATE_OPENED             = 1,
        WINDOW_STATE_ACTIVE             = 2,
        WINDOW_STATE_ICONIFIED          = 3,
        WINDOW_STATE_ACCELERATED        = 4,
        WINDOW_STATE_RED_BITS           = 5,
        WINDOW_STATE_GREEN_BITS         = 6,
        WINDOW_STATE_BLUE_BITS          = 7,
        WINDOW_STATE_ALPHA_BITS         = 8,
        WINDOW_STATE_DEPTH_BITS         = 9,
        WINDOW_STATE_STENCIL_BITS       = 10,
        WINDOW_STATE_REFRESH_RATE       = 11,
        WINDOW_STATE_ACCUM_RED_BITS     = 12,
        WINDOW_STATE_ACCUM_GREEN_BITS   = 13,
        WINDOW_STATE_ACCUM_BLUE_BITS    = 14,
        WINDOW_STATE_ACCUM_ALPHA_BITS   = 15,
        WINDOW_STATE_AUX_BUFFERS        = 16,
        WINDOW_STATE_STEREO             = 17,
        WINDOW_STATE_WINDOW_NO_RESIZE   = 18,
        WINDOW_STATE_FSAA_SAMPLES       = 19,
    };

    struct WindowParams
    {
        WindowParams()
        : m_ResizeCallback(0x0)
        , m_ResizeCallbackUserData(0x0)
        , m_CloseCallback(0x0)
        , m_CloseCallbackUserData(0x0)
        , m_Width(640)
        , m_Height(480)
        , m_Samples(1)
        , m_Title("Defold Application")
        , m_Fullscreen(false)
        , m_PrintDeviceInfo(false)
        , m_HighDPI(false) {}

        PlatformGraphicsApi     m_GraphicsApi;
        /// Window resize callback
        WindowResizeCallback    m_ResizeCallback;
        /// User data supplied to the callback function
        void*                   m_ResizeCallbackUserData;
        /// Window close callback
        WindowCloseCallback     m_CloseCallback;
        /// User data supplied to the callback function
        void*                   m_CloseCallbackUserData;
        /// Window focus callback
        WindowFocusCallback     m_FocusCallback;
        /// User data supplied to the callback function
        void*                   m_FocusCallbackUserData;
        /// Window iconify callback
        WindowIconifyCallback   m_IconifyCallback;
        /// User data supplied to the callback function
        void*                   m_IconifyCallbackUserData;
        /// Window width, 640 by default
        uint32_t                m_Width;
        /// Window height, 480 by default
        uint32_t                m_Height;
        /// Number of samples (for multi-sampling), 1 by default
        uint32_t                m_Samples;
        /// Window title, "Dynamo App" by default
        const char*             m_Title;
        /// If the window should cover the full screen or not, false by default
        bool                    m_Fullscreen;
        /// Log info about the graphics device being used, false by default
        bool                    m_PrintDeviceInfo;
        ///
        bool                    m_HighDPI;
        // Window background color, RGB 0x00BBGGRR
        uint32_t                m_BackgroundColor;
    };

    HWindow        NewWindow(const WindowParams& params);
    void           DeleteWindow(HWindow window);
    PlatformResult OpenWindow(HWindow window);
    void           CloseWindow(HWindow window);
    uint32_t       GetWindowWidth(HWindow window);
    uint32_t       GetWindowHeight(HWindow window);
    void           SetWindowSize(HWindow window, uint32_t width, uint32_t height);
    uint32_t       GetWindowState(HWindow window, WindowState state);
    void           IconifyWindow(HWindow window);
    float          GetDisplayScaleFactor(HWindow window);
};

#endif // DM_RIG_H
