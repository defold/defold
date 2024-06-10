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

#ifndef DMSDK_CONFIGFILE_H
#define DMSDK_CONFIGFILE_H

#include <stdint.h>
#include <stdbool.h>

/*# SDK ConfigFile API documentation
 *
 * Configuration file access functions.
 * The configuration file is compiled version of the [file:game.project] file.
 *
 * @document
 * @name ConfigFile
 * @path engine/dlib/src/dmsdk/dlib/configfile.h
 */

/*# HConfigFile type definition
 *
 * @typedef
 * @name HConfigFile
 */
typedef struct ConfigFile* HConfigFile;

/*# get config value as string
 *
 * Get config value as string, returns default if the key isn't found
 *
 * @name ConfigFileGetString
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:const char*] Default value to return if key isn't found
 * @return value [type:const char*] found value or default value
 *
 * @examples
 * ```c
 * static ExtensionResult AppInitialize(ExtensionAppParams* params)
 * {
 *     const char* projectTitle = ConfigFileGetString(params->m_ConfigFile, "project.title", "Untitled");
 * }
 * ```
* @examples
 *
 * ```cpp
 * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
 * {
 *     const char* projectTitle = dmConfigFile::GetString(params->m_ConfigFile, "project.title", "Untitled");
 * }
 *
 */
const char* ConfigFileGetString(HConfigFile config, const char* key, const char* default_value);

/*# get config value as integer
 *
 * Get config value as integer, returns default if the key isn't found
 *
 * @name ConfigFileGetInt
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:int32_t] Default value to return if key isn't found
 * @return value [type:int32_t] found value or default value
 *
 * @examples
 * ```c
 * static ExtensionResult AppInitialize(ExtensionAppParams* params)
 * {
 *     int32_t displayWidth = ConfigFileGetInt(params->m_ConfigFile, "display.width", 640);
 * }
 * ```
* @examples
 *
 * ```cpp
 * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
 * {
 *     int32_t displayWidth = dmConfigFile::GetInt(params->m_ConfigFile, "display.width", 640);
 * }
 *
 */
int32_t ConfigFileGetInt(HConfigFile config, const char* key, int32_t default_value);

/*# get config value as float
 *
 * Get config value as float, returns default if the key isn't found
 *
 * @name ConfigFileGetFloat
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:int32_t] Default value to return if key isn't found
 * @return value [type:int32_t] found value or default value
 *
 * @examples
 * ```c
 * static ExtensionResult AppInitialize(ExtensionAppParams* params)
 * {
 *     float gravity = ConfigFileGetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
 * }
 * ```
* @examples
 *
 * ```cpp
 * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
 * {
 *     float gravity = dmConfigFile::GetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
 * }
 *
 */
float ConfigFileGetFloat(HConfigFile config, const char* key, float default_value);

// Extensions

/*# Called when config file extension is created
 * @typedef
 * @name FConfigFileCreate
 * @param config [type:HConfigFile] Config file handle
 */
typedef void (*FConfigFileCreate)(HConfigFile config);

/*# Called when config file extension is destroyed
 * @typedef
 * @name FConfigFileDestroy
 * @param config [type:HConfigFile] Config file handle
 */
typedef void (*FConfigFileDestroy)(HConfigFile config);

/*# Called when a string is requested from the config file extension
 * @typedef
 * @name FConfigFileGetString
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:const char*] Default value to return if key isn't found
 * @param out [type:const char**] Out argument where result is stored if found. Caller must free() this memory.
 * @return result [type:bool] True if property was found
 */
typedef bool (*FConfigFileGetString)(HConfigFile config, const char* key, const char* default_value, const char** out);

/*# Called when an integer is requested from the config file extension
 * @typedef
 * @name FConfigFileGetInt
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:int32_t] Default value to return if key isn't found
 * @param out [type:int32_t*] Out argument where result is stored if found.
 * @return result [type:bool] True if property was found
 */
typedef bool (*FConfigFileGetInt)(HConfigFile config, const char* key, int32_t default_value, int32_t* out);

/*# Called when a float is requested from the config file extension
 * @typedef
 * @name FConfigFileGetFloat
 * @param config [type:HConfigFile] Config file handle
 * @param key [type:const char*] Key in format section.key (.key for no section)
 * @param default_value [type:float] Default value to return if key isn't found
 * @param out [type:float*] Out argument where result is stored if found.
 * @return result [type:bool] True if property was found
 */
typedef bool (*FConfigFileGetFloat)(HConfigFile config, const char* key, float default_value, float* out);

/*# Used when registering new config file extensions
 * @constant
 * @name ConfigFileExtensionDescBufferSize
 */
const uint32_t ConfigFileExtensionDescBufferSize = 64;

/*# Called when a float is requested from the config file extension
 * @typedef
 * @name FConfigFileGetFloat
 * @param desc [type:void*] An opaque buffer of at least ConfigFileExtensionDescBufferSize bytes.
 *                          This will hold the meta data for the plugin.
 * @param desc_size [type:uint32_t] The size of the desc buffer. Must be >ConfigFileExtensionDescBufferSize
 * @param name [type:const char*] Name of the extension.
 * @param create [type:FConfigFileCreate] Extension create function. May be null.
 * @param destroy [type:FConfigFileDestroy] Extension destroy function. May be null.
 * @param get_string [type:FConfigFileGetString] Getter for string properties. May be null.
 * @param get_int [type:FConfigFileGetInt] Getter for int properties. May be null.
 * @param get_float [type:FConfigFileGetFloat] Getter for float properties. May be null.
 */
void ConfigFileRegisterExtension(void* desc,
    uint32_t desc_size,
    const char *name,
    FConfigFileCreate create,
    FConfigFileDestroy destroy,
    FConfigFileGetString get_string,
    FConfigFileGetInt get_int,
    FConfigFileGetFloat get_float);

// internal
#define DM_REGISTER_CONFIGFILE_EXTENSION(symbol, desc, desc_size, name, create, destroy, get_string, get_int, get_float) extern "C" void symbol () { \
    ConfigFileRegisterExtension((void*) &desc, desc_size, name, create, destroy, get_string, get_int, get_float); \
}

// internal
#define DM_DMCF_PASTE_SYMREG(x, y) x ## y
// internal
#define DM_DMCF_PASTE_SYMREG2(x, y) DM_DMCF_PASTE_SYMREG(x, y)

/*# declare a new config file extension
 *
 * Declare and register new config file extension to the engine.
 * Each get function should return true if it sets a proper value. Otherwise return false.
 *
 * @macro
 * @name DM_DECLARE_CONFIGFILE_EXTENSION
 * @param symbol [type:symbol] external extension symbol description (no quotes).
 * @param name [type:const char*] extension name. Human readable.
 * @param init [type:function(dmConfigFile::dmcfConfig*)] init function. May be null.
 * @param get_string [type:function(const char* section_plus_name, const char* default, const char** out)] Gets a string property. May be null.
 * @param get_int [type:function(const char* section_plus_name, int default, int* out)] Gets an int property. May be null.
 * @param get_float [type:function(const char* section_plus_name, float default, float* out)] Gets a float property. May be null.
 *
 * @examples
 *
 * Register a new config file extension:
 *
 * ```cpp
 * DM_DECLARE_CONFIGFILE_EXTENSION(MyConfigfileExtension, "MyConfigfileExtension", create, destroy, get_string, get_int, get_float);
 * ```
 */
#define DM_DECLARE_CONFIGFILE_EXTENSION(symbol, name, create, destroy, get_string, get_int, get_float) \
    uint8_t DM_ALIGNED(16) DM_DMCF_PASTE_SYMREG2(symbol, __LINE__)[ConfigFileExtensionDescBufferSize]; \
    DM_REGISTER_CONFIGFILE_EXTENSION(symbol, DM_DMCF_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_DMCF_PASTE_SYMREG2(symbol, __LINE__)), name, create, destroy, get_string, get_int, get_float);

#if defined(__cplusplus)
   #include "configfile.hpp"
#endif

#endif // DMSDK_CONFIGFILE_H
