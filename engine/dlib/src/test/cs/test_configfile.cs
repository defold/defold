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

public unsafe class TestConfigFile
{
    private static string GetTestName()
    {
        return "test_configfile.cs";
    }
    private static int AssertEq<T>(T expected, T v)
    {
        if (expected != null && expected.Equals(v)) return 0;
        Console.WriteLine(String.Format("{0}: Error: Expected: {1}, but got {2}", GetTestName(), expected, v));
        return 1;
    }

    private static int RunTestsConfigFile(ConfigFile.Config* config)
    {
        int errors = 0;
        errors += AssertEq<int>(1, ConfigFile.GetInt(config, "test.my_int", -1));
        errors += AssertEq<float>(2.0f, ConfigFile.GetFloat(config, "test.my_float", -1.0f));
        errors += AssertEq<string>("hello", ConfigFile.GetString(config, "test.my_string", "wrong"));
        return errors; // return != 0 for failure
    }

    [UnmanagedCallersOnly(EntryPoint = "csRunTestsConfigFile")]
    public static int csRunTestsConfigFile(ConfigFile.Config* config)
    {
        // Useful for a suite of tests
        return RunTestsConfigFile(config);
    }
}
