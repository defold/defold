// Generated, do not edit!
// Generated with cwd=/Users/bjornritzl/projects/defold/engine/dlib and cmd=/Users/bjornritzl/projects/defold/scripts/dmsdk/gen_sdk.py -i /Users/bjornritzl/projects/defold/engine/dlib/sdk_gen.json

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

#ifndef DMSDK_CONFIGFILE_GEN_HPP
#define DMSDK_CONFIGFILE_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# SDK ConfigFile API documentation
 *
 * Configuration file access functions.
 * The configuration file is compiled version of the [file:game.project] file.
 *
 * @document
 * @language C++
 * @name ConfigFile
 * @namespace dmConfigFile
 */

#include <stdint.h>

#include <dmsdk/dlib/configfile.h>

namespace dmConfigFile
{
    /*# HConfigFile type definition
     * Each game session has a single config file that holds all parameters from game.project and any overridden values.
     * @typedef
     * @name HConfig
     * @language C++
     * @note Properties can be overridden on command line or via the config file extension system. (See [ref:DM_DECLARE_CONFIGFILE_EXTENSION])
     */
    typedef HConfigFile HConfig;

    /*# 
     * Get config value as string, returns default if the key isn't found
     * @name GetString
     * @language C++
     * @param config [type:HConfigFile] Config file handle
     * @param key [type:const char*] Key in format section.key (.key for no section)
     * @param default_value [type:const char*] Default value to return if key isn't found
     * @return value [type:const char*] found value or default value
     * @examples 
     * ```c
     * static ExtensionResult AppInitialize(ExtensionAppParams* params)
     * {
     *     const char* projectTitle = ConfigFileGetString(params->m_ConfigFile, "project.title", "Untitled");
     * }
     * ```
     * @examples 
     * ```cpp
     * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
     * {
     *     const char* projectTitle = dmConfigFile::GetString(params->m_ConfigFile, "project.title", "Untitled");
     * }
     * ```
     */
    const char * GetString(HConfig config, const char * key, const char * default_value);

    /*# get config value as integer
     * Get config value as integer, returns default if the key isn't found
     * @name GetInt
     * @language C++
     * @param config [type:HConfigFile] Config file handle
     * @param key [type:const char*] Key in format section.key (.key for no section)
     * @param default_value [type:int32_t] Default value to return if key isn't found
     * @return value [type:int32_t] found value or default value
     * @examples 
     * ```c
     * static ExtensionResult AppInitialize(ExtensionAppParams* params)
     * {
     *     int32_t displayWidth = ConfigFileGetInt(params->m_ConfigFile, "display.width", 640);
     * }
     * ```
     * @examples 
     * ```cpp
     * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
     * {
     *     int32_t displayWidth = dmConfigFile::GetInt(params->m_ConfigFile, "display.width", 640);
     * }
     * ```
     */
    int32_t GetInt(HConfig config, const char * key, int32_t default_value);

    /*# get config value as float
     * Get config value as float, returns default if the key isn't found
     * @name GetFloat
     * @language C++
     * @param config [type:HConfigFile] Config file handle
     * @param key [type:const char*] Key in format section.key (.key for no section)
     * @param default_value [type:int32_t] Default value to return if key isn't found
     * @return value [type:int32_t] found value or default value
     * @examples 
     * ```c
     * static ExtensionResult AppInitialize(ExtensionAppParams* params)
     * {
     *     float gravity = ConfigFileGetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
     * }
     * ```
     * @examples 
     * ```cpp
     * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
     * {
     *     float gravity = dmConfigFile::GetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
     * }
     * ```
     */
    float GetFloat(HConfig config, const char * key, float default_value);


} // namespace dmConfigFile

#endif // #define DMSDK_CONFIGFILE_GEN_HPP
/*# HConfigFile type definition
 * Each game session has a single config file that holds all parameters from game.project and any overridden values.
 * @typedef
 * @name HConfigFile
 * @language C
 * @note Properties can be overridden on command line or via the config file extension system. (See [ref:DM_DECLARE_CONFIGFILE_EXTENSION])
 */

/*# 
 * Get config value as string, returns default if the key isn't found
 * @name ConfigFileGetString
 * @language C
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:const char*] Default value to return if key isn't found
 * @return value [type:const char*] found value or default value
 * @examples 
 * ```c
 * static ExtensionResult AppInitialize(ExtensionAppParams* params)
 * {
 *     const char* projectTitle = ConfigFileGetString(params->m_ConfigFile, "project.title", "Untitled");
 * }
 * ```
 * @examples 
 * ```cpp
 * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
 * {
 *     const char* projectTitle = dmConfigFile::GetString(params->m_ConfigFile, "project.title", "Untitled");
 * }
 * ```
 */

/*# get config value as integer
 * Get config value as integer, returns default if the key isn't found
 * @name ConfigFileGetInt
 * @language C
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:int32_t] Default value to return if key isn't found
 * @return value [type:int32_t] found value or default value
 * @examples 
 * ```c
 * static ExtensionResult AppInitialize(ExtensionAppParams* params)
 * {
 *     int32_t displayWidth = ConfigFileGetInt(params->m_ConfigFile, "display.width", 640);
 * }
 * ```
 * @examples 
 * ```cpp
 * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
 * {
 *     int32_t displayWidth = dmConfigFile::GetInt(params->m_ConfigFile, "display.width", 640);
 * }
 * ```
 */

/*# get config value as float
 * Get config value as float, returns default if the key isn't found
 * @name ConfigFileGetFloat
 * @language C
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:int32_t] Default value to return if key isn't found
 * @return value [type:int32_t] found value or default value
 * @examples 
 * ```c
 * static ExtensionResult AppInitialize(ExtensionAppParams* params)
 * {
 *     float gravity = ConfigFileGetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
 * }
 * ```
 * @examples 
 * ```cpp
 * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
 * {
 *     float gravity = dmConfigFile::GetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
 * }
 * ```
 */

/*# Called when config file extension is created
 * Called when config file extension is created
 * @typedef
 * @name FConfigFileCreate
 * @language C
 * @param config [type:HConfigFile] Config file handle
 */

/*# Called when config file extension is destroyed
 * Called when config file extension is destroyed
 * @typedef
 * @name FConfigFileDestroy
 * @language C
 * @param config [type:HConfigFile] Config file handle
 */

/*# Called when a string is requested from the config file extension
 * Called when a string is requested from the config file extension
 * @typedef
 * @name FConfigFileGetString
 * @language C
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:const char*] Default value to return if key isn't found
 * @param out [type:const char**] Out argument where result is stored if found. Caller must free() this memory.
 * @return result [type:bool] True if property was found
 */

/*# Called when an integer is requested from the config file extension
 * Called when an integer is requested from the config file extension
 * @typedef
 * @name FConfigFileGetInt
 * @language C
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:int32_t] Default value to return if key isn't found
 * @param out [type:int32_t*] Out argument where result is stored if found.
 * @return result [type:bool] True if property was found
 */

/*# Called when a float is requested from the config file extension
 * Called when a float is requested from the config file extension
 * @typedef
 * @name FConfigFileGetFloat
 * @language C
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:float] Default value to return if key isn't found
 * @param out [type:float*] Out argument where result is stored if found.
 * @return result [type:bool] True if property was found
 */

/*# Used when registering new config file extensions.
 * It defines the minimum size of the description blob being registered.
 * @name ConfigFileExtensionDescBufferSize
 * @language C
 */

/*# Called when a float is requested from the config file extension
 * Called when a float is requested from the config file extension
 * @typedef
 * @name FConfigFileGetFloat
 * @language C
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:float] Default value to return if key isn't found
 * @param out [type:float*] Out argument where result is stored if found.
 * @return result [type:bool] True if property was found
 */

/*# declare a new config file extension
 * Declare and register new config file extension to the engine.
 * Each get function should return true if it sets a proper value. Otherwise return false.
 * @macro
 * @name DM_DECLARE_CONFIGFILE_EXTENSION
 * @language C
 * @examples 
 * Register a new config file extension:
 * 
 * ```cpp
 * DM_DECLARE_CONFIGFILE_EXTENSION(MyConfigfileExtension, "MyConfigfileExtension", create, destroy, get_string, get_int, get_float);
 * ```
 */


