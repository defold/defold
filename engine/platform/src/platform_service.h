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

#ifndef DM_PLATFORM_SERVICE_H
#define DM_PLATFORM_SERVICE_H

#include <dmsdk/platform/window.h>
#include <dmsdk/dlib/webserver.h>
#include <stdint.h>

namespace dmPlatformService
{
    const uint32_t MAX_REVISION_LENGTH = 32;
    const char CONTEXT_NAME[] = "platform_service";

    typedef struct Service* HService;
    typedef uint64_t        (*FGetTime)();

    struct WindowMetrics
    {
        uint32_t m_WindowWidth;
        uint32_t m_WindowHeight;
        uint32_t m_DrawableWidth;
        uint32_t m_DrawableHeight;
        float    m_DisplayScale;
        bool     m_WindowAvailable;
        bool     m_DrawableAvailable;
        // Native window-mode capability; usable resizing also requires available metrics.
        bool     m_ResizeAvailable;
    };

    // A value copy with no native handles; no HTTP request or second revision is needed.
    struct WindowObservation
    {
        char          m_Revision[MAX_REVISION_LENGTH];
        WindowMetrics m_Metrics;
    };

    // All calls run on the server/window owner thread. The server is borrowed.
    // A null clock selects the system monotonic clock. Registration failure returns null.
    HService New(dmWebServer::HServer server, FGetTime get_time = 0);
    // Unregisters the handler before freeing its context; the server must still exist.
    void Delete(HService service);
    // Every attachment invalidates geometry observations. The window is borrowed.
    void Attach(HService service, HWindow window);
    void Observe(HService service, WindowObservation* observation);
    // Only this call advances resizes. Called by the extension before a graphics frame begins.
    void Update(HService service);
    // Clears the borrowed window and receipts before native teardown.
    void Detach(HService service);
}

#endif
