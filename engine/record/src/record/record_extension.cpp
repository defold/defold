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
#include <dmsdk/engine/engine_event.h>
#include "record_window.h"
#include <dmsdk/engine/extension.hpp>
#include <dmsdk/extension/extension.hpp>
#include <dmsdk/dlib/log.h>

namespace dmRecord
{
    static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
    {
        dmWebServer::HServer server = dmEngine::GetWebServer(params);
        if (server && !RegisterService(server, GetRecordingWindow))
        {
            dmLogError("Unable to register the recording service");
        }
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Initialize(dmExtension::Params*)
    {
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result FinalizeExtension(dmExtension::Params*)
    {
        // Fallback for hosts that finalize without an engine-delete event.
        FinalizeService();
        return dmExtension::RESULT_OK;
    }

    static void OnEvent(dmExtension::Params*, dmhash_t event_id, const void*, uint32_t)
    {
        // Join native capture before sound, render and window teardown begins.
        if (event_id == dmEngine::EVENT_ENGINE_DELETE)
        {
            FinalizeService();
        }
    }

    static dmExtension::Result AppFinalize(dmExtension::AppParams*)
    {
        // Also handles initialization failures before any script context exists.
        FinalizeService();
        return dmExtension::RESULT_OK;
    }
} // namespace dmRecord

static dmExtension::Result RegisterExtension(dmExtension::HExtension extension)
{
    dmExtension::SetAppInitializeFn(extension, dmRecord::AppInitialize);
    dmExtension::SetAppFinalizeFn(extension, dmRecord::AppFinalize);
    dmExtension::SetInitializeFn(extension, dmRecord::Initialize);
    dmExtension::SetFinalizeFn(extension, dmRecord::FinalizeExtension);
    dmExtension::SetEventFn(extension, dmRecord::OnEvent);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EngineRecord, RegisterExtension, 0)
