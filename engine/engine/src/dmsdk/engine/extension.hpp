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

#ifndef DMSDK_ENGINE_EXTENSION_HPP
#define DMSDK_ENGINE_EXTENSION_HPP

#include <dmsdk/dlib/configfile.h>
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
     * @path engine/dlib/src/dmsdk/engine/extension.h
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
     * @name GetConfigFile
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return config [type:dmConfigFile::HConfig] The game project config file
     */
    dmConfigFile::HConfig   GetConfigFile(dmExtension::AppParams* app_params);

    /*# get the web server handle
     * @note Only valid in debug builds
     * @name GetWebServer
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return server [type:dmWebServer::HServer] The web server handle
     */
    dmWebServer::HServer    GetWebServer(dmExtension::AppParams* app_params);

    /*# get the game object register
     * @name GetGameObjectRegister
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return register [type:dmGameObject::HRegister] The game object register
     */
    dmGameObject::HRegister GetGameObjectRegister(dmExtension::AppParams* app_params);

    /*# get the hid context
     * @name GetHIDContext
     * @param app_params [type:dmExtension::AppParams*] The app params sent to the extension dmExtension::AppInitialize / dmExtension::AppInitialize
     * @return context [type:dmHID::HContext] The hid context
     */
    dmHID::HContext GetHIDContext(dmExtension::AppParams* app_params);
}

#endif // #ifndef DMSDK_ENGINE_EXTENSION_HPP
