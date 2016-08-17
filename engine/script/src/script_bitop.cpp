#include "script.h"
#include "script_private.h"

extern "C"
{
#include "bitop/bitop.h"
}

namespace dmScript
{
    void InitializeBitop(lua_State* L)
    {
        luaopen_bit(L);

        // Above call leaves a table and a number on the stack which will not
        // be needed for anything.
        lua_pop(L, 2);
    }
}
