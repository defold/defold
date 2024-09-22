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
