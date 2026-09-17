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


/*# SDK Hash API documentation
 *
 * Hash functions.
 *
 * @document
 * @language C#
 * @name Hash
 * @namespace Dlib
 */

using System.Runtime.InteropServices;
using System.Reflection.Emit;


namespace dmSDK.Dlib {
    public unsafe partial class Hash
    {
        /// <summary>
        /// Calculate 32-bit hash value from buffer
        /// </summary>
        /// <param name="buffer" type="const void*">Buffer</param>
        /// <param name="buffer_len" type="uint">Length of buffer</param>
        /// <returns name="hash" type="uint">hash value</returns>
        [DllImport("dlib", EntryPoint="dmHashBuffer32", CallingConvention = CallingConvention.Cdecl)]
        public static extern uint HashBuffer32(void* buffer, uint buffer_len);

        /// <summary>
        /// calculate 64-bit hash value from buffer
        /// </summary>
        /// <param name="buffer" type="const void*">Buffer</param>
        /// <param name="buffer_len" type="uint">Length of buffer</param>
        /// <returns name="hash" type="UInt64">hash value</returns>
        [DllImport("dlib", EntryPoint="dmHashBuffer64", CallingConvention = CallingConvention.Cdecl)]
        public static extern UInt64 HashBuffer64(void* buffer, uint buffer_len);

        /// <summary>
        /// Calculate 32-bit hash value from string
        /// </summary>
        /// <param name="string" type="const char*">Null terminated string</param>
        /// <returns name="hash" type="uint">hash value</returns>
        [DllImport("dlib", EntryPoint="dmHashString32", CallingConvention = CallingConvention.Cdecl)]
        public static extern uint HashString32(String str);

        /// <summary>
        /// calculate 64-bit hash value from string
        /// </summary>
        /// <param name="string" type="const char*">Null terminated string</param>
        /// <returns name="hash" type="UInt64">hash value</returns>
        [DllImport("dlib", EntryPoint="dmHashString64", CallingConvention = CallingConvention.Cdecl)]
        public static extern UInt64 HashString64(String str);

        /// <summary>
        /// Returns the original string used to produce a hash.
        /// Always returns a null terminated string. Returns "<unknown>" if the original string wasn't found.
        /// </summary>
        /// <param name="hash" type="UInt64">hash value</param>
        /// <returns name="" type="const char*">Original string value or "<unknown>" if it wasn't found.</returns>
        [DllImport("dlib", EntryPoint="dmHashReverseSafe64", CallingConvention = CallingConvention.Cdecl)]
        public static extern String HashReverseSafe64(UInt64 hash);

        /// <summary>
        /// Returns the original string used to produce a hash.
        /// Always returns a null terminated string. Returns "<unknown>" if the original string wasn't found.
        /// </summary>
        /// <param name="hash" type="uint">hash value</param>
        /// <returns name="" type="const char*">Original string value or "<unknown>" if it wasn't found.</returns>
        [DllImport("dlib", EntryPoint="dmHashReverseSafe32", CallingConvention = CallingConvention.Cdecl)]
        public static extern String HashReverseSafe32(uint hash);

    } // Hash
} // dmSDK.Dlib

