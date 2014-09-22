#include "script.h"
#include "script_private.h"

extern "C"
{
#include "luasocket/luasocket.h"
}

namespace dmScript
{
    void InitializeLuasocket(lua_State* L)
    {
        luaopen_socket_core(L);

        // above call leaves a table on the stack, which will not be
        // needed for anything, so clean up.
        lua_pop(L, 1);
    }
}
