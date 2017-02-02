#ifndef H_SCRIPT_COLLECTIONPROXY
#define H_SCRIPT_COLLECTIONPROXY

#include <dlib/hash.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmGameSystem
{

    void ScriptCollectionProxyRegister(const struct ScriptLibContext& context);
    void ScriptCollectionProxyFinalize(const struct ScriptLibContext& context);

    dmhash_t GetCollectionProxyUrlHash(lua_State* L, int i);

};

#endif // H_SCRIPT_COLLECTIONPROXY