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

#include "record_service.h"
#include <dmsdk/engine/extension.hpp>
#include <dmsdk/extension/extension.hpp>
#include <dmsdk/graphics/graphics_native.h>
#include <dmsdk/dlib/log.h>

namespace dmRecord
{
    static void* NativeWindow()
    {
#if defined(DM_PLATFORM_MACOS)
        return (void*)dmGraphics::GetNativeOSXNSWindow();
#elif defined(DM_PLATFORM_WINDOWS)
        return (void*)dmGraphics::GetNativeWindowsHWND();
#else
        return 0;
#endif
    }

    static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
    {
        dmWebServer::HServer server = dmEngine::GetWebServer(params);
        if (server && !RegisterService(server, NativeWindow))
            dmLogError("Unable to register the recording service");
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Initialize(dmExtension::Params*)
    {
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Update(dmExtension::Params*)
    {
        UpdateService();
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result FinalizeExtension(dmExtension::Params*)
    {
        // Fallback for hosts that finalize without an engine-delete event.
        FinalizeService();
        return dmExtension::RESULT_OK;
    }

    static void OnEvent(dmExtension::Params*, const dmExtension::Event* event)
    {
        // Join native capture before sound, render and window teardown begins.
        if (event->m_Event == EXTENSION_EVENT_ID_ENGINE_DELETE)
            FinalizeService();
    }

    static dmExtension::Result AppFinalize(dmExtension::AppParams*)
    {
        // Also handles initialization failures before any script context exists.
        FinalizeService();
        return dmExtension::RESULT_OK;
    }
} // namespace dmRecord

DM_DECLARE_EXTENSION(EngineRecord, "Record", dmRecord::AppInitialize, dmRecord::AppFinalize, dmRecord::Initialize, dmRecord::Update, dmRecord::OnEvent, dmRecord::FinalizeExtension)
