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
    typedef void* HContext;

    enum PlatformResult
    {
        PLATFORM_RESULT_OK                    = 0,
        PLATFORM_RESULT_WINDOW_OPEN_ERROR     = -1,
        PLATFORM_RESULT_WINDOW_ALREADY_OPENED = -2,
    };

    enum PlatformGraphicsApi
    {
        PLATFORM_GRAPHICS_API_NULL,
        PLATFORM_GRAPHICS_API_OPENGL,
        PLATFORM_GRAPHICS_API_VULKAN,
    };

    struct WindowParams
    {
        PlatformGraphicsApi m_GraphicsApi;
        uint16_t            m_Width;
        uint16_t            m_Height;
        uint8_t             m_Samples;
        uint8_t             m_HighDPI    : 1;
        uint8_t             m_FullScreen : 1;
    };

    HContext NewContext();
    void     Update(HContext context);

    HWindow        NewWindow(HContext context, const WindowParams& params);
    void           DeleteWindow(HContext context, HWindow window);
    PlatformResult OpenWindow(HContext context, HWindow window);
};

#endif // DM_RIG_H
