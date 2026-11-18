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

#include <dlib/log.h>

#include "engine_graphics.h"

namespace dmEngine
{
    static dmGraphics::HContext TryInitializeGraphics(HWindow window, const WindowCreateParams& window_params,
                                                      const dmGraphics::ContextParams& context_params)
    {
        WindowResult result = dmPlatform::OpenWindow(window, window_params);
        if (result != WINDOW_RESULT_OK)
        {
            dmLogError("Failed to open the graphics window: %d.", result);
            return 0;
        }

        return dmGraphics::NewContext(context_params);
    }

    dmGraphics::HContext InitializeGraphics(HWindow window, const WindowCreateParams& window_params,
                                            const dmGraphics::ContextParams& context_params, bool allow_fallback)
    {
        dmGraphics::HContext context = TryInitializeGraphics(window, window_params, context_params);
        if (context || !allow_fallback || dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_VULKAN)
        {
            return context;
        }

        // A Vulkan context failure must release its resources before the native
        // window is closed and the graphics function table is replaced.
        dmPlatform::CloseWindow(window);
        if (!dmGraphics::InstallAdapterFallback(dmGraphics::ADAPTER_FAMILY_OPENGL) &&
            !dmGraphics::InstallAdapterFallback(dmGraphics::ADAPTER_FAMILY_OPENGLES))
        {
            dmLogError("Vulkan initialization failed. No compatible OpenGL fallback adapter is available.");
            return 0;
        }

        dmLogWarning("Vulkan initialization failed. Retrying with '%s'.",
                     dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGL ? "OpenGL" : "OpenGL ES");

        WindowCreateParams fallback_window_params = window_params;
        fallback_window_params.m_GraphicsApi = dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGL ?
                                               WINDOW_GRAPHICS_API_OPENGL : WINDOW_GRAPHICS_API_OPENGLES;
        dmGraphics::ContextParams fallback_context_params = context_params;
        fallback_context_params.m_GraphicsApiVersionMajorHint = 0;
        fallback_context_params.m_GraphicsApiVersionMinorHint = 0;
        return TryInitializeGraphics(window, fallback_window_params, fallback_context_params);
    }

}
