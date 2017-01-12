#ifndef H_LIVEUPDATE_PRIVATE
#define H_LIVEUPDATE_PRIVATE

#define LIB_NAME "liveupdate"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmLiveUpdate
{

    void LuaInit(lua_State* L);

    int LiveUpdate_GetCurrentManifest(lua_State* L);
    int LiveUpdate_MissingResources(lua_State* L);
    int LiveUpdate_VerifyResource(lua_State* L);
    int LiveUpdate_StoreResource(lua_State* L);

};

#endif // H_LIVEUPDATE_PRIVATE