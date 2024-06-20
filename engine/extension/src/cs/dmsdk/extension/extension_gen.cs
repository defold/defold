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

using System.Runtime.InteropServices;
using System.Reflection.Emit;

using dmSDK.Dlib;
using dmSDK.Lua;

namespace dmSDK.Extension {
    public unsafe partial class Extension
    {
        /*#
        * Generated from [ref:ExtensionAppParams]
        */
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct AppParams
        {
            public ConfigFile.Config* ConfigFile;
        }

        /*#
        * Generated from [ref:ExtensionParams]
        */
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct Params
        {
            public ConfigFile.Config* ConfigFile;
            public IntPtr ResourceFactory;
            public Lua.Lua.State * L;
        }

        /*#
        * Generated from [ref:ExtensionEvent]
        */
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct Event
        {
        }

        /*#
        * Generated from [ref:ExtensionRegister]
        */
        [DllImport("extension", EntryPoint="ExtensionRegister", CallingConvention = CallingConvention.Cdecl)]
        public static extern void Register(void * desc, uint desc_size, String name, IntPtr app_initialize, IntPtr app_finalize, IntPtr initialize, IntPtr finalize, IntPtr update, IntPtr on_event);

    } // Extension
} // dmSDK.Extension

