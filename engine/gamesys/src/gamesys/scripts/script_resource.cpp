#include "script_resource.h"
#include <dlib/log.h>
#include <script/script.h>
#include "../gamesys.h"

namespace dmGameSystem
{

/*# Resource API documentation
 *
 * Functions and constants to access the resources
 *
 * @name Resource
 * @namespace resource
 */

static int Set(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    //dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);
    //dmBuffer:HBuffer = dmScript::CheckBuffer(L, 2);

    dmLogWarning("resource.set is not implemented yet!");

    return 0;
}

// Resource specific hooks
static int CreateTexture(lua_State* L)
{
    //uint64_t path_hash, struct dmBuffer::Buffer**
    dmLogWarning("resource.create_texture is not implemented yet!");
    return 0;
}



static const luaL_reg Module_methods[] =
{
    {"set", Set},
    //{"get", Get},
    {"create_texture", CreateTexture},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, "resource", Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

void ScriptResourceRegister(const ScriptLibContext& context)
{
    LuaInit(context.m_LuaState);
}

void ScriptResourceFinalize(const ScriptLibContext& context)
{
}

}
