using System.Runtime.InteropServices;
using System.Reflection.Emit;

namespace dmSDK {
    namespace Dlib {
        public unsafe class Hash
        {
            [DllImport("dlib", EntryPoint="dmHashString64", CallingConvention = CallingConvention.Cdecl)]
            public unsafe static extern UInt64 HashString64(string buffer);
        } // class Hash
    } // namespace Dlib
} // namespace dmSDK


