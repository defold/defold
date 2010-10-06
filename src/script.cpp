#include "script.h"

#include "script_hash.h"
#include "script_vmath.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    void Initialize(lua_State* L)
    {
        InitializeHash(L);
        InitializeVmath(L);
    }
}
