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

#include <dmsdk/engine/extension.hpp>

namespace dmEngine
{
    dmConfigFile::HConfig GetConfigFile(dmExtension::AppParams* app_params)
    {
        return (HConfigFile)ExtensionAppParamsGetContext(app_params, dmHashString64("config"));
    }

    dmWebServer::HServer GetWebServer(dmExtension::AppParams* app_params)
    {
        return (dmWebServer::HServer)ExtensionAppParamsGetContext(app_params, dmHashString64("webserver"));
    }

    dmGameObject::HRegister GetGameObjectRegister(dmExtension::AppParams* app_params)
    {
        return (dmGameObject::HRegister)ExtensionAppParamsGetContext(app_params, dmHashString64("register"));
    }

    dmHID::HContext GetHIDContext(dmExtension::AppParams* app_params)
    {
        return (dmHID::HContext)ExtensionAppParamsGetContext(app_params, dmHashString64("hid"));
    }
}
