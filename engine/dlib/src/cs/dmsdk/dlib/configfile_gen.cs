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


/*# SDK ConfigFile API documentation
 *
 * Configuration file access functions.
 * The configuration file is compiled version of the [file:game.project] file.
 *
 * @document
 * @language C#
 * @name ConfigFile
 * @namespace Dlib
 */

using System.Runtime.InteropServices;
using System.Reflection.Emit;


namespace dmSDK.Dlib {
    public unsafe partial class ConfigFile
    {
        /// <summary>
        /// Generated from [ref:ConfigFile]
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct Config
        {
        }

        /// <summary>
        /// Get config value as string, returns default if the key isn't found
        /// </summary>
        /// <param name="config" type="HConfigFile">Config file handle</param>
        /// <param name="key" type="const char*">Key in format section.key (.key for no section)</param>
        /// <param name="default_value" type="const char*">Default value to return if key isn't found</param>
        /// <returns name="value" type="const char*">found value or default value</returns>
        /// @examples 
        /// ```c
        /// static ExtensionResult AppInitialize(ExtensionAppParams* params)
        /// {
        ///     const char* projectTitle = ConfigFileGetString(params->m_ConfigFile, "project.title", "Untitled");
        /// }
        /// ```
        /// @examples 
        /// ```cpp
        /// static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
        /// {
        ///     const char* projectTitle = dmConfigFile::GetString(params->m_ConfigFile, "project.title", "Untitled");
        /// }
        /// ```
        [DllImport("dlib", EntryPoint="ConfigFileGetString", CallingConvention = CallingConvention.Cdecl)]
        public static extern String GetString(Config* config, String key, String default_value);

        /// <summary>
        /// Get config value as integer, returns default if the key isn't found
        /// </summary>
        /// <param name="config" type="HConfigFile">Config file handle</param>
        /// <param name="key" type="const char*">Key in format section.key (.key for no section)</param>
        /// <param name="default_value" type="int">Default value to return if key isn't found</param>
        /// <returns name="value" type="int">found value or default value</returns>
        /// @examples 
        /// ```c
        /// static ExtensionResult AppInitialize(ExtensionAppParams* params)
        /// {
        ///     int32_t displayWidth = ConfigFileGetInt(params->m_ConfigFile, "display.width", 640);
        /// }
        /// ```
        /// @examples 
        /// ```cpp
        /// static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
        /// {
        ///     int32_t displayWidth = dmConfigFile::GetInt(params->m_ConfigFile, "display.width", 640);
        /// }
        /// ```
        [DllImport("dlib", EntryPoint="ConfigFileGetInt", CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetInt(Config* config, String key, int default_value);

        /// <summary>
        /// Get config value as float, returns default if the key isn't found
        /// </summary>
        /// <param name="config" type="HConfigFile">Config file handle</param>
        /// <param name="key" type="const char*">Key in format section.key (.key for no section)</param>
        /// <param name="default_value" type="int">Default value to return if key isn't found</param>
        /// <returns name="value" type="int">found value or default value</returns>
        /// @examples 
        /// ```c
        /// static ExtensionResult AppInitialize(ExtensionAppParams* params)
        /// {
        ///     float gravity = ConfigFileGetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
        /// }
        /// ```
        /// @examples 
        /// ```cpp
        /// static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
        /// {
        ///     float gravity = dmConfigFile::GetFloat(params->m_ConfigFile, "physics.gravity_y", -9.8f);
        /// }
        /// ```
        [DllImport("dlib", EntryPoint="ConfigFileGetFloat", CallingConvention = CallingConvention.Cdecl)]
        public static extern float GetFloat(Config* config, String key, float default_value);

    } // ConfigFile
} // dmSDK.Dlib

