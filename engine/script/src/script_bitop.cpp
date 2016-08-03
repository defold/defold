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

        // above call leaves a table on the stack, which will not be
        // needed for anything, so clean up.
        lua_pop(L, 1);
        lua_pop(L, 1);
    }
}
