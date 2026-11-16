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

#ifndef DMSDK_ENGINE_EXTENSION_HPP
#define DMSDK_ENGINE_EXTENSION_HPP

#include <dmsdk/dlib/configfile.h>
#include <dmsdk/dlib/deprecated.h>
#include <dmsdk/dlib/webserver.h>
#include <dmsdk/extension/extension.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/hid/hid.h>

namespace dmEngine
{
    /*# SDK Engine extension API documentation
     *
     * @document
     * @name Engine
     * @namespace dmEngine
     * @language C++
     */

    /*# define for debug builds
     *
     * @macro
     * @name DM_DEBUG
     *
     * @examples
     * Only enable code in debug builds
     *
     * ```cpp
     * #if defined(DM_DEBUG)
     *     // ...
     * #endif
     * ```
     */

    /*# define for release builds
     *
     * @macro
     * @name DM_RELEASE
     *
     * @examples
     * Only enable code in release builds
     *
     * ```cpp
     * #if defined(DM_RELEASE)
     *     // ...
     * #endif
     * ```
     */

    /*# define for headless builds
     *
     * @macro
     * @name DM_HEADLESS
     *
     * @examples
     * Only enable code in headless builds
     *
     * ```cpp
     * #if defined(DM_HEADLESS)
     *     // ...
     * #endif
     * ```
     */

    /*# get the config file
     * @note Deprecated. Use dmExtension::AppParamsGetContextRegistry and ContextRegistryGet with CONFIGFILE_CONTEXT_NAME.
     * @name GetConfigFile
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return config [type:dmConfigFile::HConfig] The game project config file
     */
    DM_DEPRECATED("Use dmExtension::AppParamsGetContextRegistry and ContextRegistryGet with CONFIGFILE_CONTEXT_NAME instead")
    dmConfigFile::HConfig GetConfigFile(dmExtension::AppParams* app_params);

    /*# get the web server handle
     * @note Only valid in debug builds
     * @note Deprecated. Use dmExtension::AppParamsGetContextRegistry and ContextRegistryGet with WEBSERVER_CONTEXT_NAME.
     * @name GetWebServer
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return server [type:dmWebServer::HServer] The web server handle
     */
    DM_DEPRECATED("Use dmExtension::AppParamsGetContextRegistry and ContextRegistryGet with WEBSERVER_CONTEXT_NAME instead")
    dmWebServer::HServer GetWebServer(dmExtension::AppParams* app_params);

    /*# get the game object register
     * @note Deprecated. Use dmExtension::AppParamsGetContextRegistry and ContextRegistryGet with GAMEOBJECT_CONTEXT_NAME.
     * @name GetGameObjectRegister
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return register [type:dmGameObject::HRegister] The game object register
     */
    DM_DEPRECATED("Use dmExtension::AppParamsGetContextRegistry and ContextRegistryGet with GAMEOBJECT_CONTEXT_NAME instead")
    dmGameObject::HRegister GetGameObjectRegister(dmExtension::AppParams* app_params);

    /*# get the hid context
     * @note Deprecated. Use dmExtension::AppParamsGetContextRegistry and ContextRegistryGet with HID_CONTEXT_NAME.
     * @name GetHIDContext
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return context [type:dmHID::HContext] The hid context
     */
    DM_DEPRECATED("Use dmExtension::AppParamsGetContextRegistry and ContextRegistryGet with HID_CONTEXT_NAME instead")
    dmHID::HContext GetHIDContext(dmExtension::AppParams* app_params);
}

#endif // #ifndef DMSDK_ENGINE_EXTENSION_HPP
