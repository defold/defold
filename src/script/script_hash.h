#ifndef SCRIPT_HASH_H
#define SCRIPT_HASH_H

#include <stdint.h>

#include <dlib/hash.h>

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void RegisterHashLib(lua_State* L);

    void PushHash(lua_State* L, uint32_t);
    uint32_t CheckHash(lua_State* L, int index);
}

#endif // SCRIPT_HASH_H
