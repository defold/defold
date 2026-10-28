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

#include "engine_service.h"
#include "engine_service_private.h"
#include <string.h>

namespace dmEngineService
{
    void Initialize()
    {
#if defined(DM_PLATFORM_IOS)
        InitializeXcTraceProfiler();
#endif
    }
    void          Finalize() {}
    XcTraceResult StartXcTrace(const XcTraceParams&)
    {
        return XCTRACE_UNAVAILABLE;
    }
    void StopXcTrace() {}
    void GetXcTraceStatus(XcTraceStatus* status)
    {
        memset(status, 0, sizeof(*status));
        status->m_State = XCTRACE_UNSUPPORTED;
    }
} // namespace dmEngineService
