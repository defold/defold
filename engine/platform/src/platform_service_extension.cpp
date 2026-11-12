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

#include "platform_service.h"
#include <dmsdk/engine/extension.hpp>
#include <dmsdk/extension/extension.hpp>
#include <graphics/graphics.h>

namespace dmPlatformService
{
    static dmExtension::Result UpdateExtension(dmExtension::Params* params)
    {
        HService service = (HService)ContextRegistryGet(ExtensionParamsGetContextRegistry(params), CONTEXT_NAME);
        if (service)
        {
            Update(service);
        }
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
    {
        dmWebServer::HServer server = dmEngine::GetWebServer(params);
        if (!server)
        {
            return dmExtension::RESULT_OK;
        }
        HContextRegistry registry = ExtensionAppParamsGetContextRegistry(params);
        HService service = New(server);
        if (!service)
        {
            dmLogError("Unable to register the platform service");
            return dmExtension::RESULT_INIT_ERROR;
        }
        ContextRegistrySet(registry, CONTEXT_NAME, service);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        HContextRegistry registry = ExtensionParamsGetContextRegistry(params);
        HService service = (HService)ContextRegistryGet(registry, CONTEXT_NAME);
        if (service)
        {
            dmGraphics::HContext graphics = (dmGraphics::HContext)ContextRegistryGet(registry, GRAPHICS_CONTEXT_NAME);
            HWindow window = dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_NULL ? 0 : dmGraphics::GetWindow(graphics);
            Attach(service, window);
        }
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Finalize(dmExtension::Params* params)
    {
        HService service = (HService)ContextRegistryGet(ExtensionParamsGetContextRegistry(params), CONTEXT_NAME);
        if (service)
        {
            Detach(service);
        }
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result AppFinalize(dmExtension::AppParams* params)
    {
        HContextRegistry registry = ExtensionAppParamsGetContextRegistry(params);
        HService service = (HService)ContextRegistryGet(registry, CONTEXT_NAME);
        if (service)
        {
            Delete(service);
            ContextRegistrySet(registry, CONTEXT_NAME, 0);
        }
        return dmExtension::RESULT_OK;
    }
}

DM_DECLARE_EXTENSION(EnginePlatform, "Platform", dmPlatformService::AppInitialize, dmPlatformService::AppFinalize, dmPlatformService::Initialize, dmPlatformService::UpdateExtension, 0, dmPlatformService::Finalize)
