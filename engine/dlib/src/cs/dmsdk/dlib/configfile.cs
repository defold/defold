using System.Runtime.InteropServices;
using System.Reflection.Emit;

namespace dmSDK {
    namespace Dlib {
        public unsafe class ConfigFile
        {
            [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
            public struct Config
            {
                // opaque struct
            }

            [DllImport("dlib", EntryPoint="ConfigFileGetString", CallingConvention = CallingConvention.Cdecl)]
            public static extern string GetString(Config* config, string key, string default_value);

            [DllImport("dlib", EntryPoint="ConfigFileGetInt", CallingConvention = CallingConvention.Cdecl)]
            public static extern int GetInt(Config* config, string key, int default_value);

            [DllImport("dlib", EntryPoint="ConfigFileGetFloat", CallingConvention = CallingConvention.Cdecl)]
            public static extern float GetFloat(Config* config, string key, float default_value);

        } // class ConfigFile
    } // namespace Dlib
} // namespace dmSDK

