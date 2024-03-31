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
 * @namespace dmConfigFile
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

int32_t     ConfigFileGetInt(HConfigFile config, const char* key, int32_t default_value);
float       ConfigFileGetFloat(HConfigFile config, const char* key, float default_value);

// Extensions
typedef void (*FConfigFileCreate)(HConfigFile config);
typedef void (*FConfigFileDestroy)(HConfigFile config);
typedef bool (*FConfigFileGetString)(HConfigFile config, const char* key, const char* default_value, const char** out);
typedef bool (*FConfigFileGetInt)(HConfigFile config, const char* key, int32_t default_value, int32_t* out);
typedef bool (*FConfigFileGetFloat)(HConfigFile config, const char* key, float default_value, float* out);

const uint32_t ConfigFileExtensionDescBufferSize = 64;

void ConfigFileRegisterExtension(void* desc,
    uint32_t desc_size,
    const char *name,
    FConfigFileCreate create,
    FConfigFileDestroy destroy,
    FConfigFileGetString get_string,
    FConfigFileGetInt get_int,
    FConfigFileGetFloat get_float);

#define DM_REGISTER_CONFIGFILE_EXTENSION(symbol, desc, desc_size, name, create, destroy, get_string, get_int, get_float) extern "C" void __attribute__((constructor)) symbol () { \
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
