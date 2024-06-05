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
    public unsafe partial class Hash
    {
        /*#
        * Generated from [ref:dmHashBuffer32]
        */
        [DllImport("dlib", EntryPoint="dmHashBuffer32", CallingConvention = CallingConvention.Cdecl)]
        public static extern uint HashBuffer32(void* buffer, uint buffer_len);

        /*#
        * Generated from [ref:dmHashBuffer64]
        */
        [DllImport("dlib", EntryPoint="dmHashBuffer64", CallingConvention = CallingConvention.Cdecl)]
        public static extern UInt64 HashBuffer64(void* buffer, uint buffer_len);

        /*#
        * Generated from [ref:dmHashString32]
        */
        [DllImport("dlib", EntryPoint="dmHashString32", CallingConvention = CallingConvention.Cdecl)]
        public static extern uint HashString32(string str);

        /*#
        * Generated from [ref:dmHashString64]
        */
        [DllImport("dlib", EntryPoint="dmHashString64", CallingConvention = CallingConvention.Cdecl)]
        public static extern UInt64 HashString64(string str);

        /*#
        * Generated from [ref:dmHashReverseSafe64]
        */
        [DllImport("dlib", EntryPoint="dmHashReverseSafe64", CallingConvention = CallingConvention.Cdecl)]
        public static extern string HashReverseSafe64(UInt64 hash);

        /*#
        * Generated from [ref:dmHashReverseSafe32]
        */
        [DllImport("dlib", EntryPoint="dmHashReverseSafe32", CallingConvention = CallingConvention.Cdecl)]
        public static extern string HashReverseSafe32(uint hash);

    } // Hash
} // dmSDK.Dlib

