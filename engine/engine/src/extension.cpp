// Copyright 2020-2024 The Defold Foundation
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

#include "extension.h"
#include <stddef.h>
#include <dlib/static_assert.h>
#include <stddef.h> // offsetof

// The same struct is used on all platforms. Although not all platforms use C++11 or higher,
// we'll at least check on most platforms.
// (I don't want to add any runtime code to do these types of checks /MAWE)
#if __cplusplus >= 201103L
// Keeping backwards compatibility
DM_STATIC_ASSERT(offsetof(dmExtension::AppParams, m_ConfigFile) == offsetof(dmEngine::ExtensionAppParams, m_ConfigFile), Struct_Member_Offset_Mismatch);
#endif

namespace dmEngine
{
    dmConfigFile::HConfig GetConfigFile(dmExtension::AppParams* app_params)
    {
        return ((dmEngine::ExtensionAppParams*)app_params)->m_ConfigFile;
    }

    dmWebServer::HServer GetWebServer(dmExtension::AppParams* app_params)
    {
        return ((dmEngine::ExtensionAppParams*)app_params)->m_WebServer;
    }

    dmGameObject::HRegister GetGameObjectRegister(dmExtension::AppParams* app_params)
    {
        return ((dmEngine::ExtensionAppParams*)app_params)->m_GameObjectRegister;
    }

    dmHID::HContext GetHIDContext(dmExtension::AppParams* app_params)
    {
        return ((dmEngine::ExtensionAppParams*)app_params)->m_HIDContext;
    }
}
