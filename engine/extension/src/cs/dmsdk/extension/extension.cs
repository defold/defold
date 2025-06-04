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

namespace dmSDK
{
    namespace Extension
    {
        public unsafe partial class Extension
        {
            public static bool TryGetFunctionPointer(Delegate d, out IntPtr pointer)
            {
                ArgumentNullException.ThrowIfNull(d);
                var method = d.Method;

                if (d.Target is {} || !method.IsStatic || method is DynamicMethod)
                {
                    pointer = 0;
                    return false;
                }

                pointer = method.MethodHandle.GetFunctionPointer();
                return true;
            }

            public static IntPtr GetFunctionPointer(Delegate d)
            {
                ArgumentNullException.ThrowIfNull(d);
                var method = d.Method;

                if (d.Target is {} || !method.IsStatic || method is DynamicMethod)
                {
                    return (IntPtr)0;
                }

                return method.MethodHandle.GetFunctionPointer();
            }

        } // class Extension
    } // namespace Extension
} // namespace dmSDK
