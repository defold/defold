// Copyright 2020-2022 The Defold Foundation
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

namespace dmConfigFile
{
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

    /*# HConfig type definition
     *
     * ```cpp
     * typedef struct Config* HConfig;
     * ```
     *
     * @typedef
     * @name dmConfigFile::HConfig
     */
    typedef struct Config* HConfig;

    /*# get config value as string
     *
     * Get config value as string, returns default if the key isn't found
     *
     * @name dmConfigFile::GetString
     * @param config [type:dmConfigFile::HConfig] Config file handle
     * @param key [type:const char*] Key in format section.key (.key for no section)
     * @param default_value [type:const char*] Default value to return if key isn't found
     * @return value [type:const char*] found value or default value
     * @examples
     *
     * ```cpp
     * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
     * {
     *     const char* projectTitle = dmConfigFile::GetString(params->m_ConfigFile, "project.title", "Untitled");
     * }
     * ```
     */
    const char* GetString(HConfig config, const char* key, const char* default_value);

    /*# get config value as int
     *
     * Get config value as int, returns default if the key isn't found
     * Note: default_value is returned for invalid integer values
     *
     * @name dmConfigFile::GetInt
     * @param config [type:dmConfigFile::HConfig] Config file handle
     * @param key [type:const char*] Key in format section.key (.key for no section)
     * @param default_value [type:int32_t] Default value to return if key isn't found
     * @return value [type:int32_t] found value or default value
     * @examples
     *
     * ```cpp
     * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
     * {
     *     int32_t displayWidth = dmConfigFile::GetInt(params->m_ConfigFile, "display.width", 640);
     * }
     * ```
     */
    int32_t GetInt(HConfig config, const char* key, int32_t default_value);

    /*# get config value as float
     *
     * Get config value as float, returns default if the key isn't found
     * Note: default_value is returned for invalid float values
     *
     * @name dmConfigFile::GetFloat
     * @param config [type:dmConfigFile::HConfig] Config file handle
     * @param key [type:const char*] Key in format section.key (.key for no section)
     * @param default_value [type:float] Default value to return if key isn't found
     * @return value [type:float] found value or default value
     * @examples
     *
     * ```cpp
     * static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
     * {
     *     float gravity = dmConfigFile::GetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
     * }
     * ```
     */
    float GetFloat(HConfig config, const char* key, float default_value);

    /**
     * Configfile extension declaration helper. Internal function. Use DM_DECLARE_CONFIGFILE_EXTENSION
     * @param desc
     * @param desc_size bytesize of buffer holding desc
     * @param name extension name. human readble
     * @param app_init app-init function. May be null
     * @param app_finalize app-final function. May be null
     * @param initialize init function. May not be 0
     * @param finalize function. May not be 0
     * @param update update function. May be null
     * @param on_event event callback function. May be null
     */
    void Register(struct PluginDesc* desc,
        uint32_t desc_size,
        const char *name,
        void (*create)(HConfig config),
        void (*destroy)(HConfig config),
        bool (*get_string)(HConfig config, const char* key, const char* default_value, const char** out),
        bool (*get_int)(HConfig config, const char* key, int32_t default_value, int32_t* out),
        bool (*get_float)(HConfig config, const char* key, float default_value, float* out)
    );

    /**
     * Extension declaration helper. Internal
     */
    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_CONFIGFILE_EXTENSION(symbol, desc, desc_size, name, create, destroy, get_string, get_int, get_float) extern "C" void __attribute__((constructor)) symbol () { \
            dmConfigFile::Register((struct dmConfigFile::PluginDesc*) &desc, desc_size, name, create, destroy, get_string, get_int, get_float); \
        }
    #else
        #define DM_REGISTER_CONFIGFILE_EXTENSION(symbol, desc, desc_size, name, create, destroy, get_string, get_int, get_float) extern "C" void symbol () { \
            dmConfigFile::Register((struct dmConfigFile::PluginDesc*) &desc, desc_size, name, create, destroy, get_string, get_int, get_float); \
            }\
            int symbol ## Wrapper(void) { symbol(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## symbol)(void) = symbol ## Wrapper;
    #endif

    /**
    * Extension desc bytesize declaration. Internal
    */
    const uint32_t m_ExtensionDescBufferSize = 64;

    // internal
    #define DM_EXTENSION_PASTE_SYMREG(x, y) x ## y
    // internal
    #define DM_EXTENSION_PASTE_SYMREG2(x, y) DM_EXTENSION_PASTE_SYMREG(x, y)

    /*# declare a new config file extension
     *
     * Declare and register new config file extension to the engine.
     * Each get function should return true if it sets a proper value. Otherwise return false.
     *
     * @macro
     * @name DM_DECLARE_CONFIGFILE_EXTENSION
     * @param symbol [type:symbol] external extension symbol description (no quotes).
     * @param name [type:const char*] extension name. Human readable.
     * @param init [type:function(dmConfigFile::HConfig)] init function. May be null.
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
        uint8_t DM_ALIGNED(16) DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__)[dmConfigFile::m_ExtensionDescBufferSize]; \
        DM_REGISTER_CONFIGFILE_EXTENSION(symbol, DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_EXTENSION_PASTE_SYMREG2(symbol, __LINE__)), name, create, destroy, get_string, get_int, get_float);

}

#endif // DMSDK_CONFIGFILE_H
