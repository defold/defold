// Generated, do not edit!
// Generated with cwd=/Users/mathiaswesterdahl/work/defold/engine/dlib and cmd=../../scripts/dmsdk/gen_sdk.py -i ./sdk_gen.json

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

namespace dmSDK.Dlib {
    public unsafe partial class ConfigFile
    {
        /*#
        * Generated from [ref:ConfigFile]
        */
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
        public struct Config
        {
        }

        /*#
        * Generated from [ref:ConfigFileGetString]
        */
        [DllImport("dlib", EntryPoint="ConfigFileGetString", CallingConvention = CallingConvention.Cdecl)]
        public static extern string GetString(Config* config, string key, string default_value);

        /*#
        * Generated from [ref:ConfigFileGetInt]
        */
        [DllImport("dlib", EntryPoint="ConfigFileGetInt", CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetInt(Config* config, string key, int default_value);

        /*#
        * Generated from [ref:ConfigFileGetFloat]
        */
        [DllImport("dlib", EntryPoint="ConfigFileGetFloat", CallingConvention = CallingConvention.Cdecl)]
        public static extern float GetFloat(Config* config, string key, float default_value);

    } // ConfigFile
} // dmSDK.Dlib

