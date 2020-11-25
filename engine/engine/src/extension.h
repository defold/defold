// Copyright 2020 The Defold Foundation
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

#include <dmsdk/engine/extension.h>

namespace dmEngine
{
    /** application level callback data
     *
     * Extension application entry callback data.
     * This is the data structure passed as parameter by extension Application entry callbacks (AppInit and AppFinalize) functions
     *
     * @struct
     * @name dmEngine::ExtensionAppParams
     * @member m_ConfigFile [type:dmConfigFile::HConfig]
     * @member m_WebServer [type:dmWebServer::HServer] Only valid in debug builds, where the engine service is running. 0 otherwise.
     * @member dmGameObject::HRegister [type:dmWebServer::HServer] Only valid in debug builds, where the engine service is running. 0 otherwise.
     *
     */
    struct ExtensionAppParams
    {
        ExtensionAppParams() { memset(this, 0, sizeof(*this)); }
        dmConfigFile::HConfig   m_ConfigFile;
        dmWebServer::HServer    m_WebServer;
        dmGameObject::HRegister m_GameObjectRegister;
    };

}