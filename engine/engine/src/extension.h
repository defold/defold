// Copyright 2020-2025 The Defold Foundation
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

#include <dmsdk/extension/extension.h>
#include <dmsdk/dlib/configfile.h>
#include <dlib/webserver.h>
#include <gameobject/gameobject.h>
#include <dlib/hashtable.h>

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
     * @member m_GameObjectRegister [type:dmGameObject::HRegister]
     * @member m_HIDContext [type:dmHID::HContext] The
     * @member m_Contexts [type: dmHashTable64<void*>] Mappings between names and contextx
     *
     */
    struct ExtensionAppParams
    {
        ExtensionAppParams() { memset(this, 0, sizeof(*this)); }
        dmConfigFile::HConfig   m_ConfigFile;
        dmHashTable64<void*>*    m_Contexts;
        // These are extra for now.
        // However, we wish to migrate towards using the vanilla dmExtension::ExtensionAppParams instead
        dmWebServer::HServer    m_WebServer;
        dmGameObject::HRegister m_GameObjectRegister;
        dmHID::HContext         m_HIDContext;
    };

}
