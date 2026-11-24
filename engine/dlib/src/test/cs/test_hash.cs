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

using System.Runtime.InteropServices;
using System.Reflection.Emit;

using dmSDK.Dlib;

public unsafe class TestHash
{
    [UnmanagedCallersOnly(EntryPoint = "csHashString64")]
    public static UInt64 csHashString64(IntPtr _s)
    {
        string s = Marshal.PtrToStringAnsi(_s) ?? string.Empty;
        return Hash.HashString64(s);
    }

    private static int RunTestsHash()
    {
        return 0; // return != 0 for failure
    }

    [UnmanagedCallersOnly(EntryPoint = "csRunTestsHash")]
    public static int csRunTestsHash()
    {
        // Useful for a suite of tests
        return RunTestsHash();
    }
}
