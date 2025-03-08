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
