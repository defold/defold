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

#include <assert.h>
#include <dlib/log.h>

#include "engine_private.h"

namespace dmEngine
{
    static WindowsGraphicsApi AdapterFamilyToGraphicsAPI(dmGraphics::AdapterFamily family)
    {
        switch (family)
        {
            case dmGraphics::ADAPTER_FAMILY_NULL:
            {
                return WINDOW_GRAPHICS_API_NULL;
            }
            case dmGraphics::ADAPTER_FAMILY_OPENGL:
            {
                return WINDOW_GRAPHICS_API_OPENGL;
            }
            case dmGraphics::ADAPTER_FAMILY_OPENGLES:
            {
                return WINDOW_GRAPHICS_API_OPENGLES;
            }
            case dmGraphics::ADAPTER_FAMILY_VULKAN:
            {
                return WINDOW_GRAPHICS_API_VULKAN;
            }
            case dmGraphics::ADAPTER_FAMILY_VENDOR:
            {
                return WINDOW_GRAPHICS_API_VENDOR;
            }
            case dmGraphics::ADAPTER_FAMILY_WEBGPU:
            {
                return WINDOW_GRAPHICS_API_WEBGPU;
            }
            case dmGraphics::ADAPTER_FAMILY_DIRECTX:
            {
                return WINDOW_GRAPHICS_API_DIRECTX;
            }
            case dmGraphics::ADAPTER_FAMILY_METAL:
            {
                return WINDOW_GRAPHICS_API_METAL;
            }
            default:
            {
                break;
            }
        }

        assert(0);
        return (WindowsGraphicsApi) - 1;
    }

    bool IsGraphicsAdapterInstalled(WindowsGraphicsApi graphics_api)
    {
        uint32_t index = 0;
        dmGraphics::HGraphicsAdapter adapter;
        while ((adapter = dmGraphics::GetRegisteredAdapter(index++)))
        {
            if (AdapterFamilyToGraphicsAPI(dmGraphics::GetAdapterFamily(adapter)) == graphics_api)
            {
                return true;
            }
        }

        return false;
    }

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

    static bool InstallNextGraphicsAdapter(uint32_t& next_adapter)
    {
        dmGraphics::HGraphicsAdapter adapter;
        while ((adapter = dmGraphics::GetRegisteredAdapter(next_adapter++)))
        {
            if (dmGraphics::InstallAdapter(dmGraphics::GetAdapterFamily(adapter)) == dmGraphics::RESULT_OK)
            {
                return true;
            }
        }

        return false;
    }

    dmGraphics::HContext InitializeGraphics(HWindow window, const WindowCreateParams& window_params,
                                            const dmGraphics::ContextParams& context_params, bool allow_fallback)
    {
        // Initial automatic installation has already considered all adapters before
        // the installed one. Continue after it so failed adapters are never retried.
        uint32_t next_adapter = 0;
        dmGraphics::HGraphicsAdapter adapter;
        while ((adapter = dmGraphics::GetRegisteredAdapter(next_adapter++)))
        {
            if (dmGraphics::GetAdapterFamily(adapter) == dmGraphics::GetInstalledAdapterFamily())
            {
                break;
            }
        }

        do
        {
            dmGraphics::AdapterFamily family = dmGraphics::GetInstalledAdapterFamily();
            WindowCreateParams attempt_window_params = window_params;
            attempt_window_params.m_GraphicsApi = AdapterFamilyToGraphicsAPI(family);
            dmGraphics::ContextParams attempt_context_params = context_params;
            if (family != dmGraphics::ADAPTER_FAMILY_VULKAN)
            {
                attempt_context_params.m_GraphicsApiVersionMajorHint = 0;
                attempt_context_params.m_GraphicsApiVersionMinorHint = 0;
            }

            dmGraphics::HContext context = TryInitializeGraphics(window, attempt_window_params, attempt_context_params);
            if (context)
            {
                return context;
            }

            // Failed context creation must release backend resources before the
            // native window is closed and the graphics function table is replaced.
            dmPlatform::CloseWindow(window);
            if (!allow_fallback || !InstallNextGraphicsAdapter(next_adapter))
            {
                return 0;
            }

            dmLogWarning("Graphics initialization failed. Retrying with the next supported adapter.");
        } while (true);
    }

}
