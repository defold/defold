// Generated, do not edit!
// Generated with cwd=/Users/mathiaswesterdahl/work/defold/engine/extension and cmd=/Users/mathiaswesterdahl/work/defold/scripts/dmsdk/gen_sdk.py -i /Users/mathiaswesterdahl/work/defold/engine/extension/sdk_gen.json

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


/*# SDK Extension API documentation
 *
 * Functions for creating and controlling engine native extension libraries.
 *
 * @document
 * @language C#
 * @name Extension
 * @namespace Extension
 */

using System.Runtime.InteropServices;
using System.Reflection.Emit;

using dmSDK.Dlib;
using dmSDK.Lua;

namespace dmSDK.Extension {
    public unsafe partial class Extension
    {
        /// <summary>
        /// Generated from [ref:ExtensionAppParams]
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct AppParams
        {
            public ConfigFile.Config* ConfigFile;
        }

        /// <summary>
        /// The global parameters avalable when registering and unregistering an extensioin
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct Params
        {
            public ConfigFile.Config* ConfigFile;
            public IntPtr ResourceFactory;
            public Lua.Lua.State * L;
        }

        /// <summary>
        /// Generated from [ref:ExtensionEvent]
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct Event
        {
        }

        /// <summary>
        /// Extension declaration helper. Internal function. Use DM_DECLARE_EXTENSION
        /// </summary>
        /// <param name="desc" type="void*">A persistent buffer of at least 128 bytes.</param>
        /// <param name="desc_size" type="const char*">size of buffer holding desc. in bytes</param>
        /// <param name="name" type="const char*">extension name. human readble. max 16 characters long.</param>
        /// <param name="app_initialize" type="FExtensionAppInitialize">app-init function. May be null</param>
        /// <param name="app_finalize" type="FExtensionAppFinalize">app-final function. May be null</param>
        /// <param name="initialize" type="FExtensionInitialize">init function. May not be 0</param>
        /// <param name="finalize" type="FExtensionFinalize">finalize function. May not be 0</param>
        /// <param name="update" type="FExtensionUpdate">update function. May be null</param>
        /// <param name="on_event" type="FExtensionOnEvent">event callback function. May be null</param>
        [DllImport("extension", EntryPoint="ExtensionRegister", CallingConvention = CallingConvention.Cdecl)]
        public static extern void Register(void * desc, uint desc_size, String name, IntPtr app_initialize, IntPtr app_finalize, IntPtr initialize, IntPtr finalize, IntPtr update, IntPtr on_event);

    } // Extension
} // dmSDK.Extension

