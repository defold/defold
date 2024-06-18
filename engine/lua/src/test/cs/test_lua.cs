using System.Runtime.InteropServices;
using System.Reflection.Emit;

using dmSDK.Lua;

public unsafe class TestLua
{
    [UnmanagedCallersOnly(EntryPoint = "csRunTestsLua")]
    public static int csRunTestsHash(Lua.State* L)
    {
        int top = Lua.gettop(L);

        Lua.pushinteger(L, 17);
        String s = Lua.tostring(L, -1); // converts the integer into a string
        Console.WriteLine(String.Format("    tostring(17): {0}", s));
        Lua.pop(L, 1); // Pop the string

        // Create the return value
        Lua.newtable(L);

        Lua.pushstring(L, "hello");
        Lua.setfield(L, -2, "string");

        Lua.pushnumber(L, 3.0);
        Lua.setfield(L, -2, "number");

        Lua.pushinteger(L, 2);
        Lua.setfield(L, -2, "integer");

        Lua.pushnumber(L, 3.0);
        Lua.setfield(L, -2, "number");

        int newtop = Lua.gettop(L);
        return (newtop - top) == 1 ? 0 : 1; // We want to leave the table on the stack
    }
}
