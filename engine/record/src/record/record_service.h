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

#ifndef DM_RECORD_SERVICE_H
#define DM_RECORD_SERVICE_H

#include <dmsdk/dlib/webserver.h>

namespace dmRecord
{
    typedef void* (*GetNativeWindow)();

    // Adds /recording and its OpenAPI Paths Object to the existing engine server.
    // The optional UTF-8 directory is for embedders/tests; the default is application storage.
    // The host advances the shared recorder with dmRecord::Update() once per frame.
    bool RegisterService(dmWebServer::HServer server, GetNativeWindow get_window, const char* directory = 0);

    // Call on ENGINE_DELETE, before sound/render/window teardown. Safe to repeat.
    void FinalizeService();
} // namespace dmRecord

#endif
