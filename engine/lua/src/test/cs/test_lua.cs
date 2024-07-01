using System.Runtime.InteropServices;
using System.Reflection.Emit;

using dmSDK.Lua;

public unsafe class TestLua
{
    private static IntPtr GetFunctionPointer(Delegate d)
    {
        ArgumentNullException.ThrowIfNull(d);
        var method = d.Method;

        if (d.Target is {} || !method.IsStatic || method is DynamicMethod)
        {
            return (IntPtr)0;
        }

        return method.MethodHandle.GetFunctionPointer();
    }

    private static int CsLuaAdd(Lua.State* L)
    {
        int a = LuaL.checkinteger(L, 1);
        int b = LuaL.checkinteger(L, 2);
        Lua.pushinteger(L, a + b);
        return 1;
    }

    private static int CsLuaMul(Lua.State* L)
    {
        int a = LuaL.checkinteger(L, 1);
        int b = LuaL.checkinteger(L, 2);
        Lua.pushinteger(L, a * b);
        return 1;
    }

    [UnmanagedCallersOnly(EntryPoint = "csRunTestsLua")]
    public static int csRunTestsLua(Lua.State* L)
    {
        int top = Lua.gettop(L);

        // Register a new function
        LuaL.RegHelper[] functions = {
            new() {name = "add", func = GetFunctionPointer(CsLuaAdd)},
            new() {name = "mul", func = GetFunctionPointer(CsLuaMul)},
            new() {name = null, func = 0}
        };

        LuaL.Register(L, "csfuncs", functions);
        Lua.pop(L, 1);

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
